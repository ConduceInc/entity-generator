"""Conduce API utility

Usage: dataset-creator.py [--api-key=<api_key>] [--host=<hostname>] [--name=<dataset_name>] [--dataset-id=<dataset_id>] [--entities=<entities>]

Options:
    -h <hostname>, --host=<hostname>  Host to connect to [default: dev-app.conduce.com]
    --api-key=<api_key>  API authentication
    --dataset-id=<dataset_id>  Identifier of the dataset to receive data
    --entities=<entities>  JSON entity data to be sent

"""

import httplib,urllib,json
from docopt import docopt

arguments = docopt(__doc__)
connection = httplib.HTTPSConnection(arguments.get('--host'))

bearer = 'Bearer %s' %  arguments.get('--api-key')
headers = {'Content-type': 'application/json', 'Authorization': bearer}
request = {'entities': arguments.get('--entities')}
params = json.dumps(request)
connection.request("POST", "/conduce/api/datasets/add_datav2/%s" % arguments.get('--dataset-id'), params, headers)
response = connection.getresponse()
print response.status, response.reason, response.read()
connection.close()
