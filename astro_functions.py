# -*- coding: utf-8 -*-
import datetime
import pytz
from astral import Astral, Location
import logging
logger = logging.getLogger(__name__)

def isDusk(oNodeControl):
    try:
        city = oNodeControl.nodeProps.get('location', 'city')
        country = oNodeControl.nodeProps.get('location', 'country')
        lat = oNodeControl.nodeProps.get('location', 'lat')
        long = oNodeControl.nodeProps.get('location', 'long')
        localtimezone = oNodeControl.nodeProps.get('location', 'localtimezone')
        elev = oNodeControl.nodeProps.get('location', 'elev')
        a = Astral()
        a.solar_depression = 'civil'
        l = Location()
        l.name = city
        l.region = country
        if "°" in lat:
            l.latitude = lat # example: 51°31'N parsed as string
        else:
            l.latitude = float(lat) #  make it a float

        if "°" in long:
            l.longitude = long
        else:
            l.longitude = float(long)

        l.timezone = localtimezone
        l.elevation = elev
        mySun = l.sun()
        logger.debug('Dawn: %s.' % str(mySun['dawn']))
        logger.debug('Sunrise: %s.' % str(mySun['sunrise']))
        logger.debug('Sunset: %s.' % str(mySun['sunset']))
        logger.debug('Dusk: %s.' % str(mySun['dusk']))
        logger.debug("Current time %s." % str(datetime.datetime.now(pytz.timezone(oNodeControl.nodeProps.get('location', 'localtimezone')))))
        CurDateTime = datetime.datetime.now(pytz.timezone(oNodeControl.nodeProps.get('location', 'localtimezone')))
        if CurDateTime > mySun['sunset'] or CurDateTime < mySun['sunrise']: # avond en nacht
            return True
        else:
            return False

    except Exception, exp:
        logger.error("isDusk init error. Error: ", exc_info=True)