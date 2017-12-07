# -*- coding: utf-8 -*-
import string
import traceback
from twisted.internet import reactor
import Adafruit_DHT


RAINTIPVOLUME = 0.2794
STATIONELEVATION=8.7

def doUpdate(NodeControl):
    NodeControl.log.debug("Weather Status update")
    try:
        # DS18B20
        if NodeControl.nodeProps.has_option('sensordata', 'GrondSensorPath'):
            mySensorReading = getDS18B20Temp(NodeControl, NodeControl.nodeProps.get('sensordata', 'GrondSensorPath'))
            if mySensorReading is not None:
                sGrondTemp = '{:.1f}'.format(mySensorReading  / float(1000))
                NodeControl.log.debug("Grond temperatuur: %s" % sGrondTemp)
                NodeControl.MQTTPublish(sTopic="living/vloer", sValue=str(sGrondTemp), iQOS=0, bRetain=True)
            else:
                NodeControl.log.warning("no reading: %s" % NodeControl.nodeProps.get('sensordata', 'GrondSensorPath'))
        else:
            NodeControl.log.warning("Can not read buiten/grond, no [sensordata] GrondSensorPath configured")

        # DHT22
        if NodeControl.nodeProps.has_option('sensordata', 'DHT22Pin'):
            try:
                getDHT22Data(NodeControl, NodeControl.nodeProps.get('sensordata', 'DHT22Pin'), 0)
            except Exception, exp:
                NodeControl.log.warning("Error temp/hum status update, error: %s." % (traceback.format_exc()))
        else:
            NodeControl.log.warning("Can not read DHT sensor, no [sensordata] DHT22Pin configured")
        # UV Index TODO

    except Exception, exp:
        NodeControl.log.warning("Error weather status update, error: %s." % (traceback.format_exc()))

def getDHT22Data(NodeControl, Pin, iTry):
    sensor = Adafruit_DHT.DHT22
    humidity, temperature = Adafruit_DHT.read_retry(sensor, Pin)
    if humidity is not None and temperature is not None:
        Temp = '{:.1f}'.format(temperature)
        Hum = '{:.1f}'.format(humidity)
        NodeControl.MQTTPublish(sTopic="living/temp", sValue=str(Temp), iQOS=0, bRetain=True)
        NodeControl.MQTTPublish(sTopic="living/hum", sValue=str(Hum), iQOS=0, bRetain=True)
    else:
        if iTry < 15:
            reactor.callLater(2, getDHT22Data, NodeControl, Pin, iTry+1)

def getDS18B20Temp(NodeControl, sSensorPath):
    mytemp = None
    try:
        f = open(sSensorPath, 'r')
        line = f.readline() # read 1st line
        crc = line.rsplit(' ', 1)
        crc = crc[1].replace('\n', '')
        if crc == 'YES':
            line = f.readline() # read 2nd line
            mytemp = line.rsplit('t=', 1)
        else:
            NodeControl.log.warning(
                "Error reading sensor, path: %s, error: %s." % (sSensorPath, 'invalid message'))
        f.close()
        if mytemp is not None:
            return int(mytemp[1])
        else:
            return None
    except Exception, exp:
        NodeControl.log.warning("Error reading sensor, path: %s, error: %s." % (sSensorPath, traceback.format_exc()))
        return None

def checkDevices():
    # TODO
    # checks if all devices are found (does NOT check valid readings)

    allDevicesFound = False