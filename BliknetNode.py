__author__ = 'geurt'

import datetime
from twisted.internet import reactor
from twisted.internet import task

from bliknetlib import nodeControl
from serialNodesController import SerialNodesController
import livingStatus
import logging
logger = logging.getLogger(__name__)
mySerialNodesController = None
oNodeControl = None

def eUpdateLiving():
    livingStatus.doUpdate(oNodeControl)

def eUpdateNeoPixels(userdata):
    if oNodeControl.nodeProps.has_option('RGBNode', 'nodeId'):
        # mySerialNodesController.doUpdateTemp(userdata, oNodeControl.nodeProps.get('RGBNode', 'nodeId'))
        pass
    else:
        logger.info('No [RGBNode] nodeId setting not found, can not update RGB Strip')

# MQTT Things
def onMQTTSubscribe(client, userdata, mid, granted_qos):
    logger.info("Subscribed: " + str(mid) + " " + str(granted_qos))

def onMQTTMessage(client, userdata, msg):
    logger.info("ON MESSAGE:" + msg.topic + " " + str(msg.qos) + " " + str(msg.payload))
    if (msg.topic == "woonkamer/updatecmd"):
        eUpdateLiving()
    if (msg.topic == "weer/temp"):
        eUpdateNeoPixels(msg.payload)

def subscribeTopics():
    if oNodeControl.mqttClient is not None:
        oNodeControl.mqttClient.on_subscribe = onMQTTSubscribe
        oNodeControl.mqttClient.subscribe("woonkamer/updatecmd", 0);
        oNodeControl.mqttClient.subscribe("weer/temp", 0);
        oNodeControl.mqttClient.on_message = onMQTTMessage
        oNodeControl.mqttClient.loop_start()

if __name__ == '__main__':
    now = datetime.datetime.now()
    oNodeControl = nodeControl.nodeControl(r'settings/bliknetnode.conf')
    logger.info("BliknetNode: %s starting at: %s." % (oNodeControl.nodeID, now))

    # mySerialNodesController = SerialNodesController(oNodeControl)

    # upload data task
    if oNodeControl.nodeProps.has_option('living', 'active') and \
            oNodeControl.nodeProps.getboolean('living','active'):
        iUploadInt = 20
        if oNodeControl.nodeProps.has_option('living', 'uploadInterval'):
            iUploadInt = oNodeControl.nodeProps.getint('living', 'uploadInterval')
        logger.info("living upload task active, upload interval: %s" % str(iUploadInt))
        l = task.LoopingCall(eUpdateLiving)
        l.start(iUploadInt)
    else:
        logger.info("Living upload task not active.")

    subscribeTopics()

    if oNodeControl.nodeProps.has_option('watchdog', 'circusWatchDog'):
        if oNodeControl.nodeProps.getboolean('watchdog', 'circusWatchDog') == True:
            oNodeControl.circusNotifier.start()

    reactor.run()