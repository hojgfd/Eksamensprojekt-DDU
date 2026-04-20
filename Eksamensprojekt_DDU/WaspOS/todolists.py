# SPDX-License-Identifier: LGPL-3.0-or-later

import wasp
import widgets
import fonts
from micropython import const

_HOME = const(-1)
_EDIT_LIST = const(-2)
_INPUT = const(-3)

KEY_X = 0
KEY_Y = 60
KEY_W = 40
KEY_H = 36


class TodolistsApp:
    NAME = "Todo"

    def __init__(self):
        self.input_text = ""
        self.input_mode = "list"  # or "task"
        self.keys = [
            ["A","B","C","D","E","F"],
            ["G","H","I","J","K","L"],
            ["M","N","O","P","Q","R"],
            ["S","T","U","V","W","X"],
            ["Y","Z","⌫","SPACE","OK"]
        ]
        self.page = _HOME
        self.scroll = 0

        # Each todo-list = {"name": str, "tasks": [(text, done)]}
        self.lists = [
            {"name": "Default", "tasks": [("Example task", False)]}
        ]
        self.current_list = 0

    # -------------------------
    # Lifecycle
    # -------------------------
    def foreground(self):
        self.btn_add_list = widgets.Button(10, 200, 100, 35, "New List")
        self.btn_add_task = widgets.Button(120, 200, 110, 35, "New Task")
        self.btn_ok = widgets.Button(70, 200, 100, 35, "OK")
        self.btn_del_list = widgets.Button(120, 160, 110, 35, "Del List")

        self.task_checks = [widgets.Checkbox(10, 60 + i * 30) for i in range(4)]

        self._draw()

        wasp.system.request_event(
            wasp.EventMask.TOUCH |
            wasp.EventMask.SWIPE_LEFTRIGHT
        )

        self._build_keyboard()

    def background(self):
        self._save()

    def tick(self, ticks):
        pass

    # -------------------------
    # Input handling
    # -------------------------

    def _build_keyboard(self):
        self.keymap = []
        screen_w = 240
        y = KEY_Y

        for row in self.keys:
            # FIRST calculate actual row width
            total_width = 0
            widths = []

            for key in row:
                if key == "SPACE":
                    w = KEY_W * 2
                elif key == "OK":
                    w = int(KEY_W * 1.5)
                else:
                    w = KEY_W

                widths.append(w)
                total_width += w

            # Now center correctly
            start_x = max(0, (screen_w - total_width) // 2)

            x = start_x

            # Build keys
            for i, key in enumerate(row):
                w = widths[i]

                # Clamp width if it would overflow
                if x + w > 240:
                    w = 240 - x

                self.keymap.append({
                    "label": key,
                    "x": x,
                    "y": y,
                    "w": w,
                    "h": KEY_H
                })

                x += w

            y += KEY_H


    def _get_key_at(self, x, y):
        for key in self.keymap:
            if (key["x"] <= x <= key["x"] + key["w"] and
                key["y"] <= y <= key["y"] + key["h"]):
                return key["label"]
        return None
        
    def swipe(self, event):
        direction = event[0]

        if direction == wasp.EventType.DOWN:
            self.scroll += 1
        elif direction == wasp.EventType.UP:
            self.scroll = max(0, self.scroll - 1)
        else:
            wasp.system.navigate(direction)

        self._draw()

    def press(self, button, state):
        wasp.system.navigate(wasp.EventType.HOME)

    def _handle_input_touch(self, x, y):
        key = self._get_key_at(x, y)
        if not key:
            return

        if key == "⌫":
            self.input_text = self.input_text[:-1]

        elif key == "OK":
            if self.input_mode == "list":
                self.lists.append({"name": self.input_text or "List", "tasks": []})
            else:
                self.lists[self.current_list]["tasks"].append(
                    (self.input_text or "Task", False)
                )

            self.page = _HOME
            self._draw()
            return

        elif key == "SPACE":
            self.input_text += " "

        else:
            self.input_text += key

        self._draw()


    # -------------------------
    # Home screen (lists)
    # -------------------------
    def _handle_home_touch(self, x, y):
        if self.btn_add_list.touch((0, x, y)):
            self.input_text = ""
            self.input_index = 0
            self.input_mode = "list"
            self.page = _INPUT
            self._draw()
            return

        if self.btn_del_list.touch((0, x, y)):
            self._delete_list()
            return

        # select list
        for i in range(len(self.lists)):
            if 50 + i * 30 < y < 80 + i * 30:
                self.current_list = i
                self.page = _EDIT_LIST
                self._draw()
                return

    # -------------------------
    # Task screen
    # -------------------------
    def _handle_list_touch(self, x, y):
        lst = self.lists[self.current_list]["tasks"]

        # delete task if tapped right side
        for i in range(len(lst)):
            if 200 < x < 240 and 60 + i * 30 < y < 90 + i * 30:
                del lst[i]
                self._draw()
                return

        # toggle tasks
        for i, chk in enumerate(self.task_checks):
            if i < len(lst) and chk.touch((0, x, y)):
                text, done = lst[i]
                lst[i] = (text, not done)
                self._draw()
                return

        # NEW TASK → input screen
        if self.btn_add_task.touch((0, x, y)):
            self.input_text = ""
            self.input_index = 0
            self.input_mode = "task"
            self.page = _INPUT
            self._draw()
            return

        # DELETE LIST
        if self.btn_del_list.touch((0, x, y)):
            if len(self.lists) > 0:
                del self.lists[self.current_list]
                self.current_list = 0
                self.page = _HOME
                self._draw()
            return

    # -------------------------
    # Actions
    # -------------------------
    def _add_list(self):
        self.lists.append({"name": "List %d" % len(self.lists), "tasks": []})
        self._draw()

    def _delete_list(self):
        if len(self.lists) > 1:
            del self.lists[self.current_list]
            self.current_list = 0
        self._draw()
    
    def touch(self, event):
        x, y = event[1], event[2]

        if self.page == _HOME:
            self._handle_home_touch(x, y)
        elif self.page == _INPUT:
            self._handle_input_touch(x, y)
        else:
            self._handle_list_touch(x, y)

    # -------------------------
    # Drawing
    # -------------------------
    def _draw(self):
        draw = wasp.watch.drawable
        draw.fill()
        self._draw_bar()

        if self.page == _INPUT:
            self._draw_input()
        elif self.page == _HOME:
            self._draw_home()
        else:
            self._draw_tasks()

    def _draw_home(self):
        draw = wasp.watch.drawable
        draw.set_font(fonts.sans24)
        draw.string("Todo Lists", 0, 10, width=240)

        draw.set_font(fonts.sans18)
        for i, lst in enumerate(self.lists[self.scroll:self.scroll+5]):
            draw.string(lst["name"], 10, 50 + i * 30, width=220)

        self.btn_add_list.draw()
        self.btn_del_list.draw()

    def _draw_tasks(self):
        draw = wasp.watch.drawable
        lst = self.lists[self.current_list]

        draw.set_font(fonts.sans24)
        draw.string(lst["name"], 0, 10, width=240)

        draw.set_font(fonts.sans18)

        tasks = lst["tasks"][self.scroll:self.scroll+4]

        for i, (text, done) in enumerate(tasks):
            if i >= len(self.task_checks):
                break

            self.task_checks[i].state = done
            self.task_checks[i].draw()

            prefix = "[x] " if done else "[ ] "
            draw.string(prefix + text, 40, 60 + i * 30, width=200)

        self.btn_add_task.draw()
        self.btn_del_list.draw()
        
    def _draw_input(self):
        self.input_text = str(self.input_text)
        draw = wasp.watch.drawable
        draw.fill()
        self._draw_bar()

        draw.set_font(fonts.sans24)
        draw.string("Enter Name", 0, 10, width=240)

        draw.set_font(fonts.sans24)
        draw.string(self.input_text, 10, 50, width=220)

        draw.set_font(fonts.sans18)

        for key in self.keymap:
            label = key["label"]
            x = key["x"]
            y = key["y"]
            w = key["w"]
            h = key["h"]

            # center text inside key
            tx = x + w // 2 - 8
            ty = y + h // 2 - 8
            if 0 <= tx <= 240 and 0 <= ty <= 240:
                if label == "SPACE":
                    draw.string("_", tx, ty)
                else:
                    draw.string(label, tx, ty)

        #draw.set_font(fonts.sans18)
        #draw.string("Tap letters • OK", 0, 185, width=240)

    def _draw_bar(self):
        sbar = wasp.system.bar
        sbar.clock = True
        sbar.draw()

    # -------------------------
    # Persistence (optional simple)
    # -------------------------
    def _save(self):
        try:
            with open("todo.txt", "w") as f:
                for lst in self.lists:
                    f.write(lst["name"] + "|")
                    for task, done in lst["tasks"]:
                        f.write(f"{task},{int(done)};")
                    f.write("\n")
        except:
            pass