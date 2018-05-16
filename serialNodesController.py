import traceback
from time import sleep

from bliknetlib.serialNodesProtocol import SerialNodesProtocol
from bliknetlib.serialMsg import serialMsg, serialMessageSign, serialMessageType
from twisted.internet.serialport import SerialPort
from twisted.internet import reactor, task
import serial

from decimal import Decimal

import logging
logger = logging.getLogger(__name__)

class SerialNodesController(object):
    def __init__(self, oNodeControl):
        self._NodeControl = oNodeControl
        self._connectSerialPort()

    def _connectSerialPort(self):
        self.Close()
        myProtocol = SerialNodesProtocol(self._NodeControl, OnReceive=self.OnMsgReceive)
        try:
            self._serialPort = SerialPort(myProtocol, self._NodeControl.nodeProps.get('serialnodes', 'serialport'),
                                          reactor,
                                          baudrate=9600,
                                          bytesize=serial.EIGHTBITS,
                                          parity=serial.PARITY_NONE)
        except Exception, exp:
            self._NodeControl.log.error("_connectSerialPort: %s." % traceback.format_exc())

        sleep(1)

    def OnMsgReceive(self, RecMsg):
        myNodeID = self._NodeControl.nodeProps.get('system', 'nodeId')
        if str(RecMsg.ToAdress) == myNodeID:
            self._NodeControl.log("Msg for me: %s" % RecMsg)
            if int(RecMsg.Function) == 23:
                # Living concentrationPM25
                concentrationPM25 = '{:.1f}'.format(RecMsg.MsgValue)
                self._NodeControl.MQTTPublish(sTopic="living/concentrationPM25", sValue=concentrationPM25, iQOS=0, bRetain=False)
            elif int(RecMsg.Function) == 24:
                # Living concentrationPM10
                concentrationPM10 = '{:.1f}'.format(RecMsg.MsgValue)
                self._NodeControl.MQTTPublish(sTopic="living/concentrationPM10", sValue=concentrationPM10, iQOS=0, bRetain=False)
            elif int(RecMsg.Function) == 25:
                # Living AqiPM25
                AqiPM25 = '{:.1f}'.format(RecMsg.MsgValue)
                self._NodeControl.MQTTPublish(sTopic="living/AqiPM25", sValue=AqiPM25, iQOS=0, bRetain=False)
            elif int(RecMsg.Function) == 26:
                # Living AqiPM10
                AqiPM10 = '{:.1f}'.format(RecMsg.MsgValue)
                self._NodeControl.MQTTPublish(sTopic="living/AqiPM25", sValue=AqiPM10, iQOS=0, bRetain=False)
            elif int(RecMsg.Function) == 27:
                # Living AqiPM10
                AQI = '{:.1f}'.format(RecMsg.MsgValue)
                self._NodeControl.MQTTPublish(sTopic="living/AQI", sValue=AQI, iQOS=0, bRetain=False)

    def SendMessage(self, serialMessage):
        try:
            self._serialPort.write(serialMessage)
            sleep(0.1)
            return True
        except Exception, exp:
            logger.error("SendMessage error: ",exc_info=True)
            return False

    def Close(self):
        try:
            self._serialPort.loseConnection()
            self._serialPort = None
        except:
            pass