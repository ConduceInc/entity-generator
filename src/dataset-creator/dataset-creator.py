import httplib,urllib,json

connection = httplib.HTTPSConnection('dev-app.conduce.com')

headers = {'Content-type': 'application/json', 'Authorization': 'Bearer <insert_valid_key_here>'}
substrate =  {
        'x_min': -180.0, 'x_max': 180.0, 'y_min': -90.0, 'y_max': 90.0,
        'z_min': -1.0, 'z_max': 1.0, 't_min': -100000, 't_max': 1956529423000,
        'tile_levels': 1, 'x_tiles': 2, 'y_tiles': 2, 'z_tiles': 1, 't_tiles': 30, 'time_levels': 1
        }
request = {'dataset_name': 'kvl-dataset-0', 'substrate': substrate}
#params = urllib.urlencode(json.dumps(request))
params = json.dumps(request)
connection.request("POST", "/conduce/api/datasets/create", params, headers)
response = connection.getresponse()
print response.status, response.reason, response.read()
connection.close()
