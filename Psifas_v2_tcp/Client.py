import socket
import sys
from PyQt4 import QtGui, QtCore
from thread import start_new_thread
from time import sleep
import numpy as np
import cv2

# Create a TCP/IP socket
CLIENT_SOCK = socket.socket(socket.AF_INET, socket.SOCK_STREAM)


BOOM = '0'
RESER_AGC = '1'

class APP(QtGui.QMainWindow):
    
    def __init__(self):
        super(APP, self).__init__()
        self.initUI()
    
    def initUI(self):

        self.setWindowFlags(QtCore.Qt.FramelessWindowHint)
               

        self.TCP_server_port = QtGui.QLineEdit(self)
        self.TCP_server_port.setReadOnly(False);
        self.TCP_server_port.setFixedSize(50,30)
        self.TCP_server_port.move(10,10)
        self.TCP_server_port.setText("5555")
        
        self.TCP_server_ip = QtGui.QLineEdit(self)
        self.TCP_server_ip.setReadOnly(False);
        self.TCP_server_ip.setFixedSize(120,30)
        self.TCP_server_ip.move(80,10)
        self.TCP_server_ip.setText("192.168.4.1")

        self.exit = QtGui.QPushButton("X", self)
        self.exit.setFixedSize(30, 30)
        self.exit.move(360,10)
        self.exit.clicked.connect(extClicked)
        
        self.exit = QtGui.QPushButton("BOOM", self)
        self.exit.setFixedSize(250, 30)
        self.exit.move(10,70)
        self.exit.clicked.connect(lambda: self.send_cmd(BOOM))

        self.exit = QtGui.QPushButton("RESER_AGC", self)
        self.exit.setFixedSize(250, 30)
        self.exit.move(10,100)
        self.exit.clicked.connect(lambda: self.send_cmd(RESER_AGC))

                
        self.setFixedSize(400, 200)
        self.setWindowTitle('LeptonHunter')
        self.show()

        start_new_thread(self.ConnectTCP,())

    def mousePressEvent(self, event):
        self.offset = event.pos()

    def mouseMoveEvent(self, event):
        x=event.globalX()
        y=event.globalY()
        x_w = self.offset.x()
        y_w = self.offset.y()
        self.move(x-x_w, y-y_w)
        
    def ConnectTCP(self):

        # Connect the socket to the port on the server given by the caller
        server_address = (str(self.TCP_server_ip.text()), int(self.TCP_server_port.text()))
        print >>sys.stderr, 'connecting to %s port %s' % server_address
        CLIENT_SOCK.connect(server_address)
        print 'connected'

        bytes = ''
        counter = 0

        f = open('data2.dat','wb');
        packetsize = 4800
        
        while True:
            try:
                bytes += CLIENT_SOCK.recv(1)

                b_ffd1 = bytes.find('\x12\x34\x56')#flir raw

                if len(bytes) > packetsize*2:
                    bytes = ''
                    
                if b_ffd1!=-1:
                    print 'got it'
                    print b_ffd1
                    
                if b_ffd1<packetsize and b_ffd1!=-1:
                    bytes =''
                    
                if b_ffd1>=packetsize:

                    print 'frame'
                    
                    raw_8bit = bytes[b_ffd1-packetsize:b_ffd1]

                    if len(raw_8bit) != packetsize:
                        print len(raw_8bit)
                        bytes = ''
                        continue

                    
                    img_array = np.asarray(bytearray(raw_8bit), dtype=np.uint8)
                    img_mat = img_array.reshape((60, 80))
                    
                    
                    res = cv2.resize(img_mat,None,fx=6, fy=6, interpolation = cv2.INTER_CUBIC)
                    cv2.imshow('FEED',res)
                    cv2.waitKey(1)

                    bytes = ''
                                             
                                             
##                if a!=-1 and b!=-1:
##
##                    if b<a:
##                        bytes = bytes[a:]W
##                        continue
##                    
##                    jpg = bytes[a:b+2]
##                    try:
##                        i = cv2.imdecode(np.fromstring(jpg, dtype=np.uint8),cv2.CV_LOAD_IMAGE_COLOR)
##                        cv2.imshow('FEED',i)
##                        cv2.waitKey(1)
##                        
##                    except Exception as e:
##                        print e
##                        
##                    bytes = ''
                    
            except Exception as e:
                print e                    
          
    def send_cmd(self, cmd):
        try:
            CLIENT_SOCK.sendall(''.join(('$',cmd,',*','FF')));
            print ''.join(('$',cmd,',*','FF'))
        except Exception as e:
            print e
                  
def extClicked():
    CLIENT_SOCK.close()
    sys.exit()
    
def main():
    
    app = QtGui.QApplication(sys.argv)
    ex = APP()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()

