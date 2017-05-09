from PyQt4 import QtCore, QtGui, uic
import sys
import cv2
import numpy as np
from thread import start_new_thread
import time
import Queue
import socket
import copy

running = False
capture_thread = None
form_class = uic.loadUiType("main.ui")[0]
q = Queue.Queue()

def stream_thread():
    global running, q

    stream_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_address = ("192.168.4.1", 111)
    print >>sys.stderr, 'connecting to %s port %s' % server_address
    stream_client.connect(server_address)
    print 'stream connected'

    raw_8bit = ''
    
    while(running):
            try:
                raw_8bit+= stream_client.recv(4800)
                
                if len(raw_8bit) >=4800:
                    
                    raw_8bit = raw_8bit[0:4800]
                    
                    img_array = np.asarray(bytearray(raw_8bit), dtype=np.uint8)
                    img_mat = img_array.reshape((60, 80))
                    res = cv2.resize(cv2.flip(img_mat,0),None,fx=4, fy=4, interpolation = cv2.INTER_CUBIC)

                    if q.qsize() < 10:
                         q.put(cv2.cvtColor(res, cv2.COLOR_GRAY2RGB))
                    else:
                        print q.qsize()
                        
                    raw_8bit =''
                    
            except Exception as e:
                print e                
                stream_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)                
                print >>sys.stderr, 'connecting to %s port %s' % server_address
                stream_client.connect(server_address)
                print 'stream connected'
                raw_8bit = ''
                


class OwnImageWidget(QtGui.QWidget):
    def __init__(self, parent=None):
        super(OwnImageWidget, self).__init__(parent)
        self.image = None
        self.red_rect = [0,0,319,239]
        self.blue_rect = [0,0,0,0]
        self.press = [0,0]
        
    def setImage(self, image):
        self.image = image
        sz = image.size()
        self.setMinimumSize(sz)
        self.update()

    def paintEvent(self, event):
        qp = QtGui.QPainter()
        qp.begin(self)
        
        if self.image:
            
            qp.drawImage(QtCore.QPoint(0, 0), self.image)
            qp.setPen(QtGui.QColor(0, 0, 255))
            qp.drawRect(self.blue_rect[0],self.blue_rect[1],self.blue_rect[2],self.blue_rect[3])
            qp.setPen(QtGui.QColor(255, 0, 0))
            qp.drawRect(self.red_rect[0],self.red_rect[1],self.red_rect[2],self.red_rect[3])

            
        qp.end()
    
    def setRed(self, scol,srow,ecol,erow):

        self.red_rect[0] = scol*320/80
        self.red_rect[1] = srow*240/60
        self.red_rect[2] = (ecol-scol)*320/80
        self.red_rect[3] = (erow-srow)*240/60
        self.blue_rect[0] = self.red_rect[0]
        self.blue_rect[1] = self.red_rect[1]
        self.blue_rect[2] = self.red_rect[2]
        self.blue_rect[3] = self.red_rect[3]

    def getBlue(self):

        t = [0,0,0,0]
        t[0] = self.blue_rect[0]*80/320
        t[1] = self.blue_rect[1]*60/240
        t[2] = (self.blue_rect[0]+self.blue_rect[2])*80/320
        t[3] = (self.blue_rect[1]+self.blue_rect[3])*60/240
        
        return t

    def mousePressEvent(self, QMouseEvent):
         t = QMouseEvent.pos()
         self.press[0] = t.x()
         self.press[1] = t.y()

    def mouseReleaseEvent(self, QMouseEvent):
        t = QMouseEvent.pos()
        if (t.x() > 0)&(t.y() > 0):
            self.blue_rect[0] = max(10,min(min(t.x(), 310),self.press[0]))
            self.blue_rect[1] = max(10,min(min(t.y(), 230),self.press[1]))
            self.blue_rect[2] = abs(min(t.x(), 300)-self.press[0])
            self.blue_rect[3] = abs(min(t.y(), 220)-self.press[1])
            

class MyWindowClass(QtGui.QMainWindow, form_class):
    def __init__(self, parent=None):
        QtGui.QMainWindow.__init__(self, parent)
        self.setupUi(self)

        print self.positionslide.value()

        self.positionslide.sliderReleased.connect(self.send_changes)
        self.vid_holder = OwnImageWidget(self.vid_holder)       
        
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.update_frame)
        self.timer.start(1)

        self.clear_roi_btn.clicked.connect(self.send_changes_zeroroi)
        self.save_btn.clicked.connect(self.send_changes)
        self.right_btn.clicked.connect(self.turn_right)
        self.left_btn.clicked.connect(self.turn_left)
        
        self.data_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        data_thread_handle = start_new_thread(self.data_thread,())

    def turn_right(self):
        self.positionslide.setValue(self.positionslide.value()+1)
        self.send_changes()
        
    def turn_left(self):
        self.positionslide.setValue(self.positionslide.value()-1)
        self.send_changes()
        
    def update_frame(self):
        if not q.empty():
            frame = q.get()
            image = QtGui.QImage(frame, 320, 240, 320*3, QtGui.QImage.Format_RGB888)  
            self.vid_holder.setImage(image)

    def closeEvent(self, event):
        global running
        running = False

    def send_changes_zeroroi(self):
        self.vid_holder.blue_rect = [10,10,300,220]
        
    def send_changes(self):

        roi = self.vid_holder.getBlue()
        
        res = '$'
        res += str(self.record_mode.currentIndex())
        res += ','
        if self.vid_state.isChecked():
            res += '1'
        else:
            res += '0'
        res += ','
        res += str(self.fps_box.value())
        res += ','
        res += str(self.wifi_interval_box.value())
        res += ','        
        res += str(self.ssid_txt.text())
        res += ','                
        res += str(self.pw_txt.text())
        res += ','
        res += str(roi[0])
        res += ','
        res += str(roi[1])
        res += ','
        res += str(roi[2])
        res += ','
        res += str(roi[3])
        res += ','
        res += str(self.limit_txt.text())
        res += ','
        res += str(self.rec_time.value())
        res += ','
        res += str(self.positionslide.value())
        res += ',#'

        self.data_client.sendall(res)
                        
    def data_thread(self):

        global running

        server_address = ("192.168.4.1", 222)
        print >>sys.stderr, 'connecting to %s port %s' % server_address
        self.data_client.connect(server_address)
        print 'data connected'

        cmd = ''
        metric = ''
        data_started = False
        metric_started = False
        
        p_id = 0
        redbox = [0,0,0,0]
        
        while running:
            try:
                c = self.data_client.recv(1)

                if c == '@':
                    metric_started = True
                    continue
                
                if c == '$':
                    data_started = True
                    continue
                
                if c=='#':
                    if data_started:       
                        self.vid_holder.setRed(redbox[0], redbox[1], redbox[2],redbox[3])
                        data_started = False
                        cmd = ''
                        p_id = 0
                        continue
                        
                if c=='%':
                    if metric_started:
                        self.metric_txt.setText(str(metric))                        
                        metric_started = False
                        metric = ''
                        continue
                
                if metric_started:
                    metric += c
                    
                if data_started:
                    
                    if c==',':
                        
                        if cmd == '':
                            cmd = '0'
                            
                        if p_id == 0:
                            self.record_mode.setCurrentIndex(int(cmd))
                            
                        if p_id == 1:
                            self.vid_state.setChecked(int(cmd)==1)
                            
                        if p_id == 2:
                            self.fps_box.setValue(int(cmd))
                            
                        if p_id == 3:
                            self.wifi_interval_box.setValue(int(cmd))
                            
                        if p_id == 4:
                            self.ssid_txt.setText(str(cmd))

                        if p_id == 5:
                            self.pw_txt.setText(str(cmd))
                            
                        if p_id == 6:
                            redbox[0] = int(cmd)
                            
                        if p_id == 7:
                            redbox[1] = int(cmd)
                            
                        if p_id == 8:
                            redbox[2] = int(cmd)
                            
                        if p_id == 9:
                            redbox[3] = int(cmd)
                            
                        if p_id == 10:
                            self.limit_txt.setText(str(cmd))

                        if p_id == 11:
                            self.rec_time.setValue(int(cmd))
                            
                        if p_id == 12:
                            self.positionslide.setValue(int(cmd))
                        p_id += 1
                        cmd =''
                        
                    else:
                        cmd += c

            except Exception as e:
                self.data_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                print >>sys.stderr, 'connecting to %s port %s' % server_address
                self.data_client.connect(server_address)
                print 'data connected'              
                print e
            
stream_thread_handle = start_new_thread(stream_thread,())
running = True

app = QtGui.QApplication(sys.argv)
w = MyWindowClass(None)
w.setWindowTitle('Ghost Hunter')
w.show()
app.exec_()
