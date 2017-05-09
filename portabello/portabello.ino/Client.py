import socket
import sys
from PyQt4 import QtGui, QtCore
from thread import start_new_thread
from time import sleep
import numpy as np
import cv2

# Create a TCP/IP socket
stream_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

class APP(QtGui.QMainWindow):
    
    def __init__(self):
        super(APP, self).__init__()
        self.initUI()
    
    def initUI(self):

        self.setWindowFlags(QtCore.Qt.FramelessWindowHint)
        self.exit = QtGui.QPushButton("X", self)
        self.exit.setFixedSize(30, 30)
        self.exit.move(360,10)
        self.exit.clicked.connect(extClicked)

        self.setFixedSize(400, 200)
        self.setWindowTitle('LeptonHunter')
        self.show()

        start_new_thread(self.runStreamThread,())

    def mousePressEvent(self, event):
        self.offset = event.pos()

    def mouseMoveEvent(self, event):
        x=event.globalX()
        y=event.globalY()
        x_w = self.offset.x()
        y_w = self.offset.y()
        self.move(x-x_w, y-y_w)
        
    def runStreamThread(self):

        global stream_client
        server_address = ("192.168.4.1", 111)
        print >>sys.stderr, 'connecting to %s port %s' % server_address
        stream_client.connect(server_address)
        print 'stream connected'

        raw_8bit =''
        
        while True:
            try:
                raw_8bit+= stream_client.recv(4800)
                
                if len(raw_8bit) >=4800:                
                    img_array = np.asarray(bytearray(raw_8bit), dtype=np.uint8)
                    img_mat = img_array.reshape((60, 80))
                        
                    res = cv2.resize(img_mat,None,fx=6, fy=6, interpolation = cv2.INTER_CUBIC)
                    cv2.imshow('FEED',res)
                    cv2.waitKey(1)
                    raw_8bit = ''              
                    
            except Exception as e:
                print e                
                stream_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)                
                print >>sys.stderr, 'connecting to %s port %s' % server_address
                stream_client.connect(server_address)
                print 'stream connected'
def extClicked():
    stream_client.close()
    sys.exit()
    
def main():
    
    app = QtGui.QApplication(sys.argv)
    ex = APP()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()

