# SPDX-License-Identifier: LGPL-3.0-or-later
# Heart Rate Logger App for Wasp-os
#
# Made for:
# - simulator preview in VS Code / WSL
# - later upload to PineTime
#
# Notes:
# - DEMO_MODE = True gives fake changing pulse values for simulator testing
# - set DEMO_MODE = False when you are ready to wire in the real sensor call

import wasp
import fonts
import widgets
import machine

try:
    import utime as time
    ON_DEVICE = True
except ImportError:
    import time
    ON_DEVICE = False

import json
import math
from micropython import const

if ON_DEVICE:
    try:
        import ppg
    except:
        ppg = None
else:
    ppg = None

_HOME = const(0)
_RECORDING = const(1)
_RESULT = const(2)

SESSION_FILE = "hr_session.json"
DATA_FILE = "hr_data.jsonl"

_INTERVALS = (1, 2, 5)
_DURATIONS = (1, 5, 10)   # minutes

DEMO_MODE = True

class HeartratesaverApp:
    """Log heart-rate samples to the watch filesystem."""

    NAME = "HR Log"

    def __init__(self):
        self._last_sec = self._now()
        self._demo_step = 0
        self._ppg = None
        self._subsample_phase = 0
        self._ppg = None
        self._raw_count = 0
        self.page = _HOME
        self.running = False
        self._sec_timer = 0

        self.interval_idx = 1
        self.duration_idx = 1

        self._log_timer = 0
        self._log_interval = 5   # sekunder (kan ændres)
        self._ppg_ready = False

        self.total = 0
        self.remaining = 0
        self.sample_count = 0
        self.last_bpm = 1000
        self.status_text = "Ready"

        self._demo_step = 0

        self.elapsed = self.total-self.remaining
    
    def _sample_sensor_device(self):
        try:
            raw = wasp.watch.hrs.read_hrs()
            self._ppg.preprocess(raw)
            hr = self._ppg.get_heart_rate()
            return hr
        except:
            return None
    
    def _update_ppg(self):
        try:
            # 3 samples per tick → ~24Hz

            t = machine.Timer(id=1, period=8000000)
            t.start()
            self._ppg.preprocess(raw)
            wasp.system.keep_awake()

            while t.time() < 41666:
                pass
            self._ppg.preprocess(raw)

            while t.time() < 83332:
                pass
            self._ppg.preprocess(raw)

            t.stop()
            del t

            if len(self._ppg.data) > 200:
                return self._ppg.get_heart_rate()

        except:
            return None


    def _now(self):
            try:
                return int(time.time())
            except:
                pass

            try:
                return int(time.ticks_ms() // 1000)
            except:
                pass
            return 0
    # ---------------- Lifecycle ----------------

    def foreground(self):
        wasp.watch.hrs.enable()
        self.interval_btn = widgets.Button(25, 95, 190, 35, "INTERVAL")
        self.duration_btn = widgets.Button(25, 140, 190, 35, "DURATION")
        self.start_btn = widgets.Button(25, 190, 190, 35, "START")
        self.stop_btn = widgets.Button(25, 190, 190, 35, "STOP")
        #self.clear_btn = widgets.Button(25, 190, 190, 35, "CLEAR DATA")

        self._restore_session()
        self._draw()

        wasp.system.request_event(
            wasp.EventMask.TOUCH |
            wasp.EventMask.BUTTON
        )
        wasp.system.request_tick(1000 // 8)

    def background(self):
        try:
            if not self.running:
                wasp.watch.hrs.disable()
        except:
            pass

        self.interval_btn = None
        self.duration_btn = None
        self.start_btn = None
        self.stop_btn = None
        #self.clear_btn = None

    def press(self, button, state):
        wasp.system.navigate(wasp.EventType.HOME)

    def _demo_log(self, now):
        interval = _INTERVALS[self.interval_idx]
        elapsed = self.total - self.remaining

        # simulate stable but changing heart rate
        self.last_bpm = 60 + int(20 * math.sin(self._demo_step / 10))
        self._demo_step += 1

        if elapsed > 0 and (elapsed % interval) == 0:
            self.sample_count += 1
            self.status_text = "Saved (demo)"

            self._append_measurement(self.last_bpm)

            self._update_session()

    def _sensor_log(self, now):
        if not self._ppg_ready or self.last_bpm <= 0:
            return

        interval = _INTERVALS[self.interval_idx]
        elapsed = self.total - self.remaining

        if elapsed > 0 and (elapsed % interval) == 0:
            self.sample_count += 1
            self.status_text = "Saved"

            self._append_measurement({
                "t": now,
                "bpm": self.last_bpm
            })

            self._update_session()


    def _log_stable_bpm(self):
        interval = _INTERVALS[self.interval_idx]

        # kun log hvert X sekund
        if (self._now() % interval) != 0:
            return

        #if self.last_bpm <= 0:
        #    return

        self.sample_count += 1
        self.status_text = "Saved"

        self._append_measurement({
            "t": self._now(),
            "bpm": self.last_bpm
        })

        self._update_session()

    def _subtick(self):
        """Read one raw sample and feed it into the PPG processor."""
        if self._ppg is None:
            return
        try:
            raw = wasp.watch.hrs.read_hrs()
            self._ppg.preprocess(raw)
        except:
            pass

    def tick(self, ticks):
        if self.page != _RECORDING or not self.running:
            return

        wasp.system.keep_awake()

        # --- 1) Sample at 24 Hz using the same timer trick as HeartApp ---
        t = machine.Timer(id=1, period=8000000)
        t.start()
        self._subtick()          # sample 1 at ~0 ms

        while t.time() < 41666:
            pass
        self._subtick()          # sample 2 at ~41 ms

        while t.time() < 83332:
            pass
        self._subtick()          # sample 3 at ~83 ms

        t.stop()
        del t

        # --- 2) Try to get a BPM reading (needs ≥240 samples, like HeartApp) ---
        if self._ppg is not None and len(self._ppg.data) >= 240:
            bpm = self._ppg.get_heart_rate()
            if bpm is not None and 30 < bpm < 220:   # sanity check
                self.last_bpm = 100 if DEMO_MODE else bpm
                self._ppg_ready = True

        # --- 3) 1 Hz countdown and logging ---
        now = self._now()
        if now == self._last_sec:
            return
        self._last_sec = now

        if self.remaining > 0:
            self.remaining -= 1

        if self.remaining <= 0:
            self._finish_session()
            self._draw()
            return


        if DEMO_MODE:
            self._demo_log(now)
        else:
            self._sensor_log(now)

        '''if self._ppg_ready and self.last_bpm > 0:
            interval = _INTERVALS[self.interval_idx]
            elapsed = self.total - self.remaining

            if elapsed > 0:
                if (elapsed % interval) == 0:
                    self.sample_count += 1
                    self.status_text = "Saved"
                    self._append_measurement({"t": now, "bpm": self.last_bpm})
                    self._update_session()
        else:
            self.status_text = "Measuring"'''

        self._draw()
        

    def touch(self, event):
        if self.page == _HOME:
            if self.interval_btn.touch(event):
                self.interval_idx = (self.interval_idx + 1) % len(_INTERVALS)
                self._draw()
                return

            if self.duration_btn.touch(event):
                self.duration_idx = (self.duration_idx + 1) % len(_DURATIONS)
                self._draw()
                return

            if self.start_btn.touch(event):
                self._start_session()
                self._draw()
                return

            # if self.clear_btn.touch(event):
            #     self._delete_measurements()
            #     self.status_text = "Data cleared"
            #     self._draw()
            #     return

        elif self.page == _RECORDING:
            if self.stop_btn.touch(event):
                self._finish_session()
                self._draw()
                return

        elif self.page == _RESULT:
            self._reset()
            self._draw()

    # ---------------- Session control ----------------

    def _start_session(self):
        # CLEAR OLD DATA FILE
        try:
            f = open(DATA_FILE, "w")
            f.close()
        except:
            pass
        now = self._now()
        self.total = _DURATIONS[self.duration_idx] * 60
        self.remaining = self.total
        self.sample_count = 0
        self.last_bpm = 0
        self.running = True
        self.page = _RECORDING
        self.status_text = "Measuring"
        self._ppg_ready = False
        self._demo_step = 0

        if ppg:
            try:
                first = wasp.watch.hrs.read_hrs()   # seed value, same as HeartApp
                self._ppg = ppg.PPG(first)
            except:
                self._ppg = None

        self._save_session({
            "start": now,
            "total": self.total,
            "sample_count": 0,
            "interval_idx": self.interval_idx,
            "duration_idx": self.duration_idx
        })

    def _finish_session(self):
        try:
            wasp.watch.hrs.disable()
        except:
            pass

        self._ppg = None
        self.running = False
        self.page = _RESULT
        self.status_text = "Done"
        self._delete_session()

    def _reset(self):
        self.page = _HOME
        self.running = False
        self.total = 0
        self.remaining = 0
        self.sample_count = 0
        self.last_bpm = 100
        self.status_text = "Ready"

    # ---------------- Storage ----------------

    def _save_session(self, data):
        try:
            with open(SESSION_FILE, "w") as f:
                f.write(json.dumps(data))
        except:
            pass

    def _restore_session(self):
        try:
            with open(SESSION_FILE, "r") as f:
                data = json.loads(f.read())
        except:
            return

        self.total = data.get("total", 0)
        self.sample_count = data.get("sample_count", 0)
        self.interval_idx = data.get("interval_idx", 1)
        self.duration_idx = data.get("duration_idx", 1)

        self.elapsed = self._now() - data["start"]
        self.remaining = self.total - self.elapsed

        if self.remaining <= 0:
            self.remaining = 0
            self.running = False
            self.page = _RESULT
            self.status_text = "Done"
            self._delete_session()
        else:
            self.running = True
            self.page = _RECORDING
            self.status_text = "Restored"

    def _update_session(self):
        self._save_session({
            "start": self._now() - (self.total - self.remaining),
            "total": self.total,
            "sample_count": self.sample_count,
            "interval_idx": self.interval_idx,
            "duration_idx": self.duration_idx
        })

    def _delete_session(self):
        try:
            import os
            os.remove(SESSION_FILE)
        except:
            try:
                import uos
                uos.remove(SESSION_FILE)
            except:
                pass

    def _append_measurement(self, hr):
        try:
            with open(DATA_FILE, "a") as f:
                f.write('{"hr":' + str(int(hr)) + '}\n')
        except:
            self.status_text = "Write error"

    def _count_measurements(self):
        count = 0
        try:
            with open(DATA_FILE, "r") as f:
                for _ in f:
                    count += 1
        except:
            return 0
        return count

    def _delete_measurements(self):
        try:
            import os
            os.remove(DATA_FILE)
        except:
            try:
                import uos
                uos.remove(DATA_FILE)
            except:
                pass


    # ---------------- Drawing ----------------

    def _draw(self):
        draw = wasp.watch.drawable
        draw.fill()

        if self.page == _HOME:
            self._draw_home(draw)
        elif self.page == _RECORDING:
            self._draw_recording(draw)
        elif self.page == _RESULT:
            self._draw_result(draw)

    def _draw_home(self, draw):
        saved = self._count_measurements()

        draw.set_font(fonts.sans24)
        draw.string("HR Logger", 35, 18, width=170)

        draw.set_font(fonts.sans18)
        draw.string("Interval: {}s".format(_INTERVALS[self.interval_idx]), 35, 55, width=170)
        draw.string("Duration: {}m".format(_DURATIONS[self.duration_idx]), 35, 75, width=170)
        #draw.string("Saved points: {}".format(saved), 35, 230 - 110, width=170)

        self.interval_btn.draw()
        self.duration_btn.draw()
        self.start_btn.draw()
        #self.clear_btn.draw()

    def _draw_recording(self, draw):
        minutes = self.remaining // 60
        seconds = self.remaining % 60

        draw.set_font(fonts.sans24)
        draw.string("Recording", 40, 18, width=160)

        draw.set_font(fonts.sans28)
        draw.string("{:02d}:{:02d}".format(minutes, seconds), 45, 55, width=150)

        draw.set_font(fonts.sans18)
        draw.string("BPM: {}".format(self.last_bpm if self.last_bpm > 0 else "None"), 30, 110, width=180)
        draw.string("Samples: {}".format(self.sample_count), 30, 135, width=180)
        draw.string("Mode: {}".format("Demo" if DEMO_MODE else "Sensor"), 30, 160, width=180)
        draw.string("Status: {}".format(self.status_text), 30, 185, width=180)

        # progress bar
        bar_x = 20
        bar_y = 210
        bar_w = 200
        bar_h = 10

        draw.fill(0x4208, bar_x, bar_y, bar_w, bar_h)

        if self.total > 0:
            progress = (self.total - self.remaining) / self.total
        else:
            progress = 0

        fill_w = int(bar_w * progress)
        if fill_w > 0:
            draw.fill(0xffff, bar_x, bar_y, fill_w, bar_h)

        self.stop_btn.draw()

    def _draw_result(self, draw):
        draw.set_font(fonts.sans24)
        draw.string("Session Done", 25, 30, width=190)

        draw.set_font(fonts.sans18)
        draw.string("Samples: {}".format(self.sample_count), 50, 85, width=140)
        draw.string("Last BPM: {}".format(self.last_bpm), 50, 115, width=140)
        draw.string("Saved to file", 55, 145, width=130)
        draw.string("Tap to return", 50, 205, width=140)
