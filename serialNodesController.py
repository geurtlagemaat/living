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
        self._serialPort = SerialPort(myProtocol, self._NodeControl.nodeProps.get('serialnodes', 'serialport'),
                                      reactor,
                                      baudrate=9600,
                                      bytesize=serial.EIGHTBITS,
                                      parity=serial.PARITY_NONE)
        sleep(1)

    def OnMsgReceive(self, RecMsg):
        myNodeID = self._NodeControl.nodeProps.get('system', 'nodeId')
        if str(RecMsg.ToAdress) == myNodeID:
            self._NodeControl.log("Msg for me: %s" % RecMsg)

    def doUpdateTemp(self, userdata, ToNode):
        myNodeID=self._NodeControl.nodeProps.get('system', 'nodeId')
        mySetTempMsg = serialMsg(FromAdress=int(myNodeID),
                                 ToAdress=int(ToNode),
                                 Function=int(60),
                                 MsgType=serialMessageSign.ENQ)
        roundTemp = int(round(Decimal(userdata)))
        mySetTempMsg.DecPos=3
        if roundTemp>0:
            mySetTempMsg.Sign=serialMessageSign.POSITIVE
            mySetTempMsg.MsgValue = roundTemp
        else:
            mySetTempMsg.Sign = serialMessageSign.NEGATIVE
            mySetTempMsg.MsgValue = -roundTemp
        self.SendMessage(mySetTempMsg.serialMsgToString())

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