import wasp

class HelloApp:
    NAME = "Test2"

    def __init__(self):
        pass

    def foreground(self):
        draw = wasp.watch.drawable
        draw.fill()
        draw.string("HELLO WORLD", 0, 100, width=240)
