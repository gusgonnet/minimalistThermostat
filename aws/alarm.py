from __future__ import print_function

from datetime import datetime
import urllib2
import boto3

client = boto3.client('sns')

SITE = 'https://api.particle.io/v1/devices/<YOUR_PHOTON_DEVICE_ID>/currentTemp'  # URL of the site to check
EXPECTED = '"connected": true'  # String expected to be on the response
MAX_TEMPERATURE = 24

# source: http://www.dotnetperls.com/between-before-after-python
def between(value, a, b):
    # Find and validate before-part.
    pos_a = value.find(a)
    if pos_a == -1: return ""
    # Find and validate after part.
    pos_b = value.rfind(b)
    if pos_b == -1: return ""
    # Return middle part.
    adjusted_pos_a = pos_a + len(a)
    if adjusted_pos_a >= pos_b: return ""
    return value[adjusted_pos_a:pos_b]


# source: http://www.dotnetperls.com/between-before-after-python
def before(value, a):
    # Find first part and return slice before it.
    pos_a = value.find(a)
    if pos_a == -1: return ""
    return value[0:pos_a]
    
    
def validate(response):

    # get and print the response of the Particle Cloud
    resp = response.read()
    print ("Particle Cloud response: %s" % resp)
    
    # check that the reponse contains "connected": true so we know the Particle is connected
    if not (EXPECTED in resp):
        print ("The Minimalist Thermostat is offline!")
        send_email('The Minimalist Thermostat is offline!')
        return
    
    # check that the temperature is lower than a threshold
    temperature = between(resp, '"result": "', '"coreInfo')
    temperature = before(temperature, '"')

    print("The current temperature is: %s" % temperature)
    
    float_temp = float(temperature)
    if ( float_temp > MAX_TEMPERATURE):
        print ("The Minimalist Thermostat: Your house is getting too hot!")
        send_email('The Minimalist Thermostat: Your house is getting too hot!')
        return
    
    return


def send_email(message):
    response = client.publish(
        TopicArn='<YOUR_SNS_ARN_ID>',
        Message=message
    )
    

def lambda_handler(event, context):

    print('Checking {} at {}...'.format(SITE, event['time']))

    # make a string with the request type in it:
    method = "GET"
    # create a handler. you can specify different handlers here (file uploads etc)
    # but we go for the default
    handler = urllib2.HTTPHandler()
    # create an openerdirector instance
    opener = urllib2.build_opener(handler)
    # build a request
    request = urllib2.Request(SITE)
    # add any other information you want
    request.add_header("Authorization",'Bearer <YOUR_PARTICLE_ACCESS_TOKEN>')
    # overload the get method function with a small anonymous function...
    request.get_method = lambda: method

    try:
        validate(opener.open(request))
    except:
        print('The Minimalist Thermostat hit trouble! Connection failed')
        send_email('The Minimalist Thermostat hit trouble! Connection failed')
        raise
    else:
        print('Test passed!')
        return event['time']
    finally:
        print('Check complete at {}'.format(str(datetime.now())))
        