"""Capture dialog with parent=self (BEFORE fix) — dark background leaks in."""
import sys
import os
from PySide6.QtWidgets import QApplication, QMainWindow, QFileDialog
from PySide6.QtCore import QDir, QTimer

app = QApplication(sys.argv)

win = QMainWindow()
win.setStyleSheet("background-color: rgb(0, 0, 17);")
win.setWindowTitle("YOLO Label")
win.resize(800, 600)
win.show()
app.processEvents()

dialog = QFileDialog(win, "Open YOLO ONNX Model", QDir.homePath(), "ONNX Models (*.onnx)")
dialog.setOption(QFileDialog.Option.DontUseNativeDialog, True)
dialog.resize(640, 480)
dialog.show()
app.processEvents()

def grab():
    app.processEvents()
    pixmap = dialog.grab()
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "before_fix.png")
    pixmap.save(path)
    print(f"Saved {path}", flush=True)
    os._exit(0)

QTimer.singleShot(500, grab)
QTimer.singleShot(5000, lambda: os._exit(1))
app.exec()
