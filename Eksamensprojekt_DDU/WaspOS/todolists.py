# SPDX-License-Identifier: LGPL-3.0-or-later
import wasp
import widgets
import fonts
import time
from micropython import const

_HOME = const(-1)
_EDIT_LIST = const(-2)
_INPUT = const(-3)

KEY_X = 0
KEY_Y = 70
KEY_W = 70
KEY_H = 35


class TodolistsApp:
    NAME = "Todo"

    def __init__(self):
        self.input_text = ""
        self.input_mode = "list"  # or "task"
        self.keys = [
            ["1","2","3"],
            ["4","5","6"],
            ["7","8","9"],
            ["BK","0","TYPE"],
            ["OK"]
        ]

        self.t9_map = {
        "1": ".,!?",
        "2": "ABC",
        "3": "DEF",
        "4": "GHI",
        "5": "JKL",
        "6": "MNO",
        "7": "PQRS",
        "8": "TUV",
        "9": "WXYZ",
        "0": " "
    }

        self.t9_labels = {
        "1": ".,!?",
        "2": "ABC",
        "3": "DEF",
        "4": "GHI",
        "5": "JKL",
        "6": "MNO",
        "7": "PQRS",
        "8": "TUV",
        "9": "WXYZ",
        "0": "SPACE"
    }


        self.page = _HOME
        self.scroll = 0
        

        self.pending_key = None
        self.pending_index = 0

        # Each todo-list = {"name": str, "tasks": [(text, done)]}
        self.lists = [
            {"name": "Default", "tasks": [("Example task", False)]}
        ]
        self.current_list = 0

    # -------------------------
    # Lifecycle
    # -------------------------
    def foreground(self):
        self.btn_add_list = widgets.Button(10, 200, 100, 35, "New")
        self.btn_add_task = widgets.Button(120, 200, 110, 35, "Add")
        self.btn_ok = widgets.Button(70, 200, 100, 35, "OK")
        self.btn_del_list = widgets.Button(120, 160, 110, 35, "Del List")

        self.task_checks = [widgets.Checkbox(10, 60 + i * 30) for i in range(4)]
        
        self._build_keyboard()
        self._draw()

        wasp.system.request_event(
            wasp.EventMask.TOUCH |
            wasp.EventMask.SWIPE_LEFTRIGHT
        )

        self._build_keyboard()

    # -------------------------
    # Input handling
    # -------------------------

    def _build_keyboard(self):
        self.keymap = []
        screen_w = 240
        y = KEY_Y

        for row in self.keys:
            total_width = len(row) * KEY_W
            start_x = (screen_w - total_width) // 2

            x = start_x

            for key in row:
                self.keymap.append({
                    "label": key,
                    "x": x,
                    "y": y,
                    "w": KEY_W,
                    "h": KEY_H
                })
                x += KEY_W

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

        if key == "bk":
            if self.pending_key:
                self.pending_key = None
            else:
                self.input_text = self.input_text[:-1]

        elif key == "TYPE":
            if self.pending_key:
                letter = self.t9_map[self.pending_key][self.pending_index]
                self.input_text += letter
                self.pending_key = None

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

        elif key in self.t9_map:
            letters = self.t9_map[key]

            if key == self.pending_key:
                self.pending_index = (self.pending_index + 1) % len(letters)
            else:
                self.pending_key = key
                self.pending_index = 0

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

        '''if self.btn_del_list.touch((0, x, y)):
            self._delete_list()
            return'''

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

        # NEW TASK
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
        #self._draw_bar()

        if self.page == _INPUT:
            self._draw_input()
        elif self.page == _HOME:
            self._draw_home()
        else:
            self._draw_tasks()

    def _draw_input_text(self):
        draw = wasp.watch.drawable

        draw.fill(0x0000, 0, 40, 240, 30)

        draw.set_font(fonts.sans24)

        text = self.input_text
        if self.pending_key:
            letter = self.t9_map[self.pending_key][self.pending_index]
            text += "[" + letter + "]"
        if len(text) > 12:
            text = text[-12:]

        draw.string(text + "_", 10, 50, width=220)

    def _draw_keyboard(self):
        draw = wasp.watch.drawable

        for key in self.keymap:
            label = key["label"]
            x = key["x"]
            y = key["y"]
            w = key["w"]
            h = key["h"]

            w = min(w, 240 - x)
            h = min(h, 240 - y)
            if w <= 0 or h <= 0:
                continue

            is_active = (label == self.pending_key)
            color = 0x07E0 if is_active else 0x39E7

            draw.fill(color, x, y, w, h)

            draw.fill(0x0000, x, y, w, 1)
            draw.fill(0x0000, x, y+h-1, w, 1)
            draw.fill(0x0000, x, y, 1, h)
            draw.fill(0x0000, x+w-1, y, 1, h)

            draw.set_font(fonts.sans24)

            text_x = x + (w - len(label) * 12) // 2
            draw.string(label, text_x, y + 2)

            if label in self.t9_labels:
                draw.set_font(fonts.sans18)
                small = self.t9_labels[label]
                draw.string(small, x + w//2 - (len(small)*6), y + h - 16)

    def _draw_home(self):
        draw = wasp.watch.drawable
        draw.set_font(fonts.sans24)
        draw.string("Todo Lists", 0, 10, width=240)

        draw.set_font(fonts.sans18)
        for i, lst in enumerate(self.lists[self.scroll:self.scroll+5]):
            draw.string(lst["name"], 10, 50 + i * 30, width=220)

        self.btn_add_list.draw()

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

            draw.string(text, 40, 65 + i * 30, width=200)

        self.btn_add_task.draw()
        self.btn_del_list.draw()
        
    def _draw_input(self):
        draw = wasp.watch.drawable
        draw.fill()
        self._draw_bar()

        draw.set_font(fonts.sans24)
        #draw.string("Enter Name", 0, 4, width=240)

        # keyboard (kun fuld redraw når vi går ind i input)
        self._draw_keyboard()

        # tekstfelt
        self._draw_input_text()


    def _draw_bar(self):
        sbar = wasp.system.bar
        sbar.clock = True
        sbar.draw()
