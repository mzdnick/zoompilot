import math
import numpy as np
import time
import wave


from openpilot.cereal import log, messaging, custom
from openpilot.common.basedir import BASEDIR
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import Ratekeeper
from openpilot.common.utils import retry
from openpilot.common.swaglog import cloudlog

from openpilot.system import micd
from openpilot.common.hardware import HARDWARE

from openpilot.sunnypilot.selfdrive.ui.quiet_mode import QuietMode

SAMPLE_RATE = 48000
SAMPLE_BUFFER = 4096 # (approx 100ms)
MAX_VOLUME = 1.0
MIN_VOLUME = 0.1
ALERT_RAMP_TIME = 4 # seconds to ramp to max volume for warningImmediate
SELFDRIVE_STATE_TIMEOUT = 5 # 5 seconds
FILTER_DT = 1. / (micd.SAMPLE_RATE / micd.FFT_SAMPLES)

AMBIENT_DB = 26 # DB where MIN_VOLUME is applied
DB_SCALE = 30 # AMBIENT_DB + DB_SCALE is where MAX_VOLUME is applied

VOLUME_BASE = 20
if HARDWARE.get_device_type() == "tizi":
  AMBIENT_DB = 30
  VOLUME_BASE = 10

AudibleAlert = log.SelfdriveState.AudibleAlert
AudibleAlertSP = custom.SelfdriveStateSP.AudibleAlert

# AlertVolumeBoost: 0-100 user gain in dB. Lifts current_volume toward full
# scale (quiet cabins) and adds a fixed boost (loud cabins). 0 = stock.
ALERT_VOLUME_BOOST_MAX = 100
ALERT_VOLUME_BOOST_DB = 6.0  # max additive boost at 100%

# Urgent safety alerts: always stock, never altered by Volume Boost.
# Both ship at ~0 dBFS, so boost could only distort them.
BOOST_EXCLUDED_ALERTS = frozenset({AudibleAlert.warningSoft, AudibleAlert.warningImmediate})


sound_list_sp: dict[int, tuple[str, int | None, float]] = {
  # AudibleAlertSP, file name, play count (none for infinite)
  AudibleAlertSP.promptSingleLow: ("prompt_single_low.wav", 1, MAX_VOLUME),
  AudibleAlertSP.promptSingleHigh: ("prompt_single_high.wav", 1, MAX_VOLUME),
}

sound_list: dict[int, tuple[str, int | None, float]] = {
  # AudibleAlert, file name, play count (none for infinite)
  AudibleAlert.engage: ("engage.wav", 1, MAX_VOLUME),
  AudibleAlert.disengage: ("disengage.wav", 1, MAX_VOLUME),
  AudibleAlert.refuse: ("refuse.wav", 1, MAX_VOLUME),

  AudibleAlert.prompt: ("warning.wav", 1, MAX_VOLUME),
  AudibleAlert.promptRepeat: ("warning.wav", None, MAX_VOLUME),
  AudibleAlert.promptDistracted: ("dm_warning.wav", None, MAX_VOLUME),

  AudibleAlert.preAlert: ("pre_alert.wav", 1, MAX_VOLUME),
  AudibleAlert.complete: ("complete.wav", 1, MAX_VOLUME),

  AudibleAlert.warningSoft: ("critical.wav", None, MAX_VOLUME),
  AudibleAlert.warningImmediate: ("dm_critical.wav", None, MAX_VOLUME),

  **sound_list_sp,
}
if HARDWARE.get_device_type() == "tizi":
  sound_list.update({
    AudibleAlert.engage: ("engage_tizi.wav", 1, MAX_VOLUME),
    AudibleAlert.disengage: ("disengage_tizi.wav", 1, MAX_VOLUME),
  })

def check_selfdrive_timeout_alert(sm):
  ss_missing = time.monotonic() - sm.recv_time['selfdriveState']

  if ss_missing > SELFDRIVE_STATE_TIMEOUT:
    if (sm['selfdriveState'].enabled or sm['selfdriveStateSP'].mads.enabled) and (ss_missing - SELFDRIVE_STATE_TIMEOUT) < 10:
      return True

  return False


class Soundd(QuietMode):
  def __init__(self):
    super().__init__()

    self.alert_volume_boost: int = self._read_alert_volume_boost()

    self.load_sounds()

    self.current_alert = AudibleAlert.none
    self.current_volume = MIN_VOLUME
    self.current_volume_boosted = MIN_VOLUME
    self.current_sound_frame = 0

    self.ramp_start_volume = MIN_VOLUME
    self.ramp_start_time = 0.

    self.selfdrive_timeout_alert = False
    self.pending_stop = False

    self.spl_filter_weighted = FirstOrderFilter(0, 2.5, FILTER_DT, initialized=False)

  def _read_alert_volume_boost(self) -> int:
    try:
      value = int(self.params.get("AlertVolumeBoost", return_default=True) or "0")
    except (ValueError, TypeError):
      return 0
    return max(0, min(ALERT_VOLUME_BOOST_MAX, value))

  def load_param(self) -> None:
    super().load_param()
    self.alert_volume_boost = self._read_alert_volume_boost()

  def load_sounds(self):
    self.loaded_sounds: dict[int, np.ndarray] = {}
    self.loaded_sound_peaks: dict[int, float] = {}

    # Load all sounds
    for sound in sound_list:
      filename, play_count, volume = sound_list[sound]

      with wave.open(BASEDIR + "/openpilot/selfdrive/assets/sounds/" + filename, 'r') as wavefile:
        assert wavefile.getnchannels() == 1
        assert wavefile.getsampwidth() == 2
        assert wavefile.getframerate() == SAMPLE_RATE

        length = wavefile.getnframes()
        data = np.frombuffer(wavefile.readframes(length), dtype=np.int16).astype(np.float32) / (2**16/2)
        self.loaded_sounds[sound] = data
        self.loaded_sound_peaks[sound] = float(np.max(np.abs(data))) or 1.0  # source peak (1.0 if silent)

  def get_sound_data(self, frames): # get "frames" worth of data from the current alert sound, looping when required

    ret = np.zeros(frames, dtype=np.float32)

    if self.should_play_sound(self.current_alert):
      num_loops = sound_list[self.current_alert][1]
      sound_data = self.loaded_sounds[self.current_alert]
      written_frames = 0

      current_sound_frame = self.current_sound_frame % len(sound_data)
      loops = self.current_sound_frame // len(sound_data)

      while written_frames < frames and (num_loops is None or loops < num_loops):
        available_frames = sound_data.shape[0] - current_sound_frame
        frames_to_write = min(available_frames, frames - written_frames)
        ret[written_frames:written_frames+frames_to_write] = sound_data[current_sound_frame:current_sound_frame+frames_to_write]
        written_frames += frames_to_write
        self.current_sound_frame += frames_to_write
        current_sound_frame = self.current_sound_frame % len(sound_data)
        loops = self.current_sound_frame // len(sound_data)
        if self.pending_stop and current_sound_frame == 0:
          self.current_alert = AudibleAlert.none
          self.pending_stop = False
          break

    if self.current_alert in BOOST_EXCLUDED_ALERTS:
      volume = self.current_volume
    else:
      # cap at the sound's clean headroom: never exceed full scale, never shape
      volume = min(self.current_volume_boosted, 1.0 / self.loaded_sound_peaks.get(self.current_alert, 1.0))
    out = ret * volume
    return out

  def callback(self, data_out: np.ndarray, frames: int, time, status) -> None:
    if status:
      cloudlog.warning(f"soundd stream over/underflow: {status}")
    data_out[:frames, 0] = self.get_sound_data(frames)

  def update_alert(self, new_alert):
    current_alert_played_once = self.current_alert == AudibleAlert.none or self.current_sound_frame >= len(self.loaded_sounds[self.current_alert])
    # let looping sounds finish the current loop instead of cutting off mid tone
    if new_alert == AudibleAlert.none and self.current_alert != AudibleAlert.none and sound_list[self.current_alert][1] is None:
      if current_alert_played_once:
        self.pending_stop = True
      else:
        self.current_alert = AudibleAlert.none
        self.current_sound_frame = 0
      return
    self.pending_stop = False
    if self.current_alert != new_alert and (new_alert != AudibleAlert.none or current_alert_played_once):
      if new_alert == AudibleAlert.warningImmediate:
        self.ramp_start_volume = self.current_volume
        self.ramp_start_time = time.monotonic()
      self.current_alert = new_alert
      self.current_sound_frame = 0

  def get_audible_alert(self, sm):
    if sm.updated['selfdriveState']:
      new_alert = sm['selfdriveState'].alertSound.raw
      self.update_alert(new_alert)
    elif check_selfdrive_timeout_alert(sm):
      self.update_alert(AudibleAlert.warningImmediate)
      self.selfdrive_timeout_alert = True
    elif self.selfdrive_timeout_alert:
      self.update_alert(AudibleAlert.none)
      self.selfdrive_timeout_alert = False

  def calculate_volume(self, weighted_db):
    volume = ((weighted_db - AMBIENT_DB) / DB_SCALE) * (MAX_VOLUME - MIN_VOLUME) + MIN_VOLUME
    return math.pow(VOLUME_BASE, (np.clip(volume, MIN_VOLUME, MAX_VOLUME) - 1))

  @retry(attempts=10, delay=3)
  def get_stream(self, sd):
    # reload sounddevice to reinitialize portaudio
    sd._terminate()
    sd._initialize()
    return sd.OutputStream(channels=1, samplerate=SAMPLE_RATE, callback=self.callback, blocksize=SAMPLE_BUFFER)

  def soundd_thread(self):
    # sounddevice must be imported after forking processes
    import sounddevice as sd

    sm = messaging.SubMaster(['selfdriveState', 'selfdriveStateSP', 'soundPressure'])

    with self.get_stream(sd) as stream:
      rk = Ratekeeper(20)

      cloudlog.info(f"soundd stream started: {stream.samplerate=} {stream.channels=} {stream.dtype=} {stream.device=}, {stream.blocksize=}")
      while True:
        sm.update(0)

        self.load_param()

        # freeze volume during alerts to avoid mic feedback increasing volume
        if sm.updated['soundPressure']:
          self.spl_filter_weighted.update(sm["soundPressure"].soundPressureWeightedDb)
          if self.current_alert == AudibleAlert.none:
            self.current_volume = self.calculate_volume(float(self.spl_filter_weighted.x))

        self.get_audible_alert(sm)

        # Ramp up immediate warning sound over 4s
        if self.current_alert == AudibleAlert.warningImmediate:
          elapsed = time.monotonic() - self.ramp_start_time
          ramp_vol = float(np.interp(elapsed, [0, ALERT_RAMP_TIME], [self.ramp_start_volume, MAX_VOLUME]))
          self.current_volume = max(self.current_volume, ramp_vol)

        # dB-domain boost of current_volume (log10/pow stay here, out of the
        # audio callback). Exclusion + per-sound peak cap remain in get_sound_data.
        if self.alert_volume_boost and self.current_volume > 0:
          cv_db = 20.0 * math.log10(self.current_volume)
          v_frac = self.alert_volume_boost / 100.0
          eff_db = max(cv_db * (1.0 - v_frac), cv_db + v_frac * ALERT_VOLUME_BOOST_DB)
          self.current_volume_boosted = math.pow(10.0, eff_db / 20.0)
        else:
          self.current_volume_boosted = self.current_volume

        rk.keep_time()

        assert stream.active


def main():
  s = Soundd()
  s.soundd_thread()


if __name__ == "__main__":
  main()
