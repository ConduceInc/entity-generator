"""Conduce API utility

Usage: dataset-creator.py [--api-key=<api_key>] [--host=<hostname>] [--name=<dataset_name>]

Options:
    -h <hostname>, --host=<hostname>  Host to connect to [default: dev-app.conduce.com]
    --api-key=<api_key> API authentication
    -n <dataset_name>, --name=<dataset_name>  Name of new dataset

"""

import httplib,urllib,json
from docopt import docopt

arguments = docopt(__doc__)
connection = httplib.HTTPSConnection(arguments.get('--host'))

print arguments.get('--api-key')
bearer = 'Bearer %s' %  arguments.get('--api-key')
headers = {'Content-type': 'application/json', 'Authorization': bearer}
substrate =  {
        'x_min': -180.0, 'x_max': 180.0, 'y_min': -90.0, 'y_max': 90.0,
        'z_min': -1.0, 'z_max': 1.0, 't_min': -100000, 't_max': 1956529423000,
        'tile_levels': 1, 'x_tiles': 2, 'y_tiles': 2, 'z_tiles': 1, 't_tiles': 30, 'time_levels': 1
        }
request = {'name': arguments.get('--dataset_name')}
params = json.dumps(request)
connection.request("POST", "/conduce/api/datasets/createv2", params, headers)
response = connection.getresponse()
print response.status, response.reason, response.read()
connection.close()
