# SPDX-License-Identifier: LGPL-3.0-or-later
# Focus Pomodoro App for WaspOS
import wasp
import fonts
import widgets
import time
import json
from micropython import const

# Pages
_HOME = const(0)
_RUNNING = const(1)
_RESULT = const(2)
SESSION_KEY = "focus_session_v1"
SESSION_FILE = "focus_session.json"


# Durations (minutes)
_DURATIONS = (15, 25, 35, 45)

class FocusmodeApp:
    """Pomodoro-style focus timer with distraction tracking."""

    NAME = "Focus"
    # ICON = (
    #    b'\x02`@'  # simple placeholder icon (can be replaced)
    #)

    def __init__(self):
        self.page = _HOME
        self.duration_idx = 0
        self.remaining = 0
        self.distractions = 0
        self.running = False


    # ---------------- Storage -----------------
    def _store(self):
        return getattr(wasp.system, "persistent", None)
        
    def _delete_session(self):
        try:
            import uos
            uos.remove(SESSION_FILE)
        except:
            pass

    def _restore_session(self):
        import time

        try:
            with open(SESSION_FILE, "r") as f:
                data = json.loads(f.read())
        except:
            return

        self.total = data["total"]
        self.distractions = data.get("distractions", 0)

        elapsed = int(time.time() - data["start"])
        self.remaining = self.total - elapsed

        if self.remaining <= 0:
            self.remaining = 0
            self.page = _RESULT
            self.running = False
            self._delete_session()
        else:
            self.page = _RUNNING
            self.running = True

    def _save_session(self, data):
        try:
            with open(SESSION_FILE, "w") as f:
                f.write(json.dumps(data))
        except:
            pass

    def _update_session(self):
        import time

        try:
            with open(SESSION_FILE, "r") as f:
                data = json.loads(f.read())

            data["distractions"] = self.distractions

            with open(SESSION_FILE, "w") as f:
                f.write(json.dumps(data))
        except:
            pass
    # ---------------- Lifecycle ----------------

    def foreground(self):
        self.change_btn = widgets.Button(40, 130, 160, 40, "CHANGE")
        self.start_btn = widgets.Button(70, 180, 100, 40, "START")
        self.distraction_btn = widgets.Button(40, 150, 160, 40, "+1 DIST")

        self._draw()

        wasp.system.request_event(
            wasp.EventMask.TOUCH |
            wasp.EventMask.BUTTON
        )
        wasp.system.request_tick(1000)
        self._restore_session()

    def background(self):
        self.running = False
        self.page = _HOME

        self.start_btn = None
        self.next_btn = None
        self.distraction_btn = None

    # ---------------- Timer logic ----------------

    def tick(self, ticks):
        if self.page == _RUNNING and self.running:
            if self.remaining > 0:
                self.remaining -= 1
            else:
                self._finish_session()
            self._draw()

    def press(self, button, state):
        wasp.system.navigate(wasp.EventType.HOME)

    # ---------------- Input ----------------

    def touch(self, event):
        if self.page == _HOME:

            if self.change_btn.touch(event):
                self.duration_idx = (self.duration_idx + 1) % len(_DURATIONS)
                self._draw()
                return

            if self.start_btn.touch(event):
                self._start_session()
                return

        elif self.page == _RUNNING:
            if self.distraction_btn.touch(event):
                self.distractions += 1
                '''store = self._store()
                if store:
                    data = store.get(SESSION_KEY)
                    if data:
                        data["distractions"] = self.distractions
                        store.set(SESSION_KEY, data)'''
                self._update_session()
                self._draw()
                return

        elif self.page == _RESULT:
            self._reset()
            self._draw()

    # ---------------- Session control ----------------

    def _start_session(self):
        import time

        self.total = _DURATIONS[self.duration_idx] * 60
        self.remaining = self.total
        self.distractions = 0
        self.running = True
        self.page = _RUNNING

        now = time.time()

        self._save_session({
            "start": now,
            "total": self.total,
            "distractions": 0
        })

    def _finish_session(self):
        self.running = False
        self.page = _RESULT

        store = self._store()
        if store:
            store.delete(SESSION_KEY)

    def _reset(self):
        self.page = _HOME
        self.remaining = 0
        self.distractions = 0
        self.running = False

    # ---------------- Scoring ----------------

    def _score(self):
        # Base score depends on duration completion
        base = 100

        # each distraction reduces score
        penalty = self.distractions * 10

        score = base - penalty
        if score < 0:
            score = 0
        return score

    # ---------------- Rendering ----------------

        
    def _draw(self):
        draw = wasp.watch.drawable
        draw.fill()

        if self.page == _HOME:
            self._draw_home(draw)
        elif self.page == _RUNNING:
            self._draw_running(draw)
        elif self.page == _RESULT:
            self._draw_result(draw)

    def _draw_home(self, draw):
        # Title
        draw.set_font(fonts.sans24)
        draw.string("Focus Mode", 30, 30, width=180)

        # Label
        draw.set_font(fonts.sans18)
        draw.string("Session length:", 40, 60, width=160)

        # Selected time
        draw.set_font(fonts.sans28)
        mins = _DURATIONS[self.duration_idx]
        
        draw.set_font(fonts.sans28)
        draw.string("{}:00".format(mins), 60, 85, width=120)


        # Buttons (moved down)
        self.change_btn.draw()
        self.start_btn.draw()

    def _draw_running(self, draw):
        
        minutes = self.remaining // 60
        seconds = self.remaining % 60

        # Time text
        draw.set_font(fonts.sans28)
        text = "{:02d}:{:02d}".format(minutes, seconds)
        draw.string(text, 0, 60, width=240)

        # Progress bar
        progress = 1 - (self.remaining / self.total)

        bar_x = 10
        bar_y = 120
        bar_width = 220
        bar_height = 12

        # Background bar (empty)
        draw.fill(0x4208, bar_x, bar_y, bar_width, bar_height)

        # Filled part
        filled_width = int(bar_width * progress)
        draw.fill(0xffff, bar_x, bar_y, filled_width, bar_height)

        # Top border
        draw.fill(0xffff, bar_x, bar_y - 1, bar_width, 1)

        # Bottom border
        draw.fill(0xffff, bar_x, bar_y + bar_height, bar_width, 1)

        # Left border
        draw.fill(0xffff, bar_x - 1, bar_y, 1, bar_height)

        # Right border
        draw.fill(0xffff, bar_x + bar_width, bar_y, 1, bar_height)

        # Distractions
        draw.set_font(fonts.sans18)
        draw.string("Distractions: {}".format(self.distractions), 20, 150, width=220)

        self.distraction_btn.draw()

    def _draw_result(self, draw):
        draw.set_font(fonts.sans24)
        draw.string("Session Done", 30, 40, width=210)

        draw.set_font(fonts.sans18)
        draw.string("Distractions: {}".format(self.distractions), 40, 90, width=200)

        draw.set_font(fonts.sans28)
        draw.string("Score: {}".format(self._score()), 60, 140, width=180)

        draw.set_font(fonts.sans18)
        draw.string("Tap to restart", 50, 200, width=190)
