import conduce.api
import conduce.config
import subprocess
import os
import sys
import json


def initialize_test(kind, args):
    resource_info = conduce.api.create_dataset(
        'generated-test-pattern', host=args.host, api_key=args.api_key)
    dataset_id = resource_info['id']

    lens_template_name = 'generated-test-pattern-dots'
    lens_template = json.load(open('dots_template.json'))

    lens_template['name'] = lens_template_name
    lens_template['dataset_id'] = dataset_id

    # TODO: Some fancy iteration over the template to catch all occurrences of "kinds"
    lens_template['kinds'] = [kind]
    lens_template['renderings'][0]['rule']['kinds'] = [kind]
    lens_template['hover_info'][0]['kinds'] = [kind]

    conduce.api.create_template(
        lens_template_name, lens_template, host=args.host, api_key=args.api_key)

    return dataset_id


def get_dataset_id(args):
    dataset_id = None

    dataset_list = conduce.api.find_dataset(
        name=args.dataset_name, host=args.host, api_key=args.api_key)

    if len(dataset_list) == 1:
        dataset_id = datatset_list[0]['id']
    else:
        print 'More than one dataset found with name {}'.format(dataset_name)
        print dataset_list

    return dataset_id


def check_dataset_id(args):
    resource_list = conduce.api.find_dataset(
        id=args.dataset_id, host=args.host, api_key=args.api_key)
    return len(resource_list) == 1


def main():
    import argparse

    default_host = None
    default_user = None

    cfg = conduce.config.get_full_config()
    if cfg:
        default_host = cfg.get('default-host')
        default_user = cfg.get('default-user')
        if default_host and default_user:
            default_api_key = conduce.config.get_api_key(
                default_user, default_host)

    arg_parser = argparse.ArgumentParser(
        description='Generate and ingest an entity test pattern.  This tool can be used to generate a live stream or flood the ingest interface with data.  By default, a new dataset is created along with a lens template for viewing the data.  Then it generates and ingests test pattern data.  If the user provides a dataset ID or dataset name the dataset and lens template are not created.')
    arg_parser.add_argument(
        '--user', help='The user that will access Conduce.  This argument is used to indirectly identify an API key to use.', default=default_user)
    arg_parser.add_argument(
        '--host', help='The server on which the command will run', default=default_host)
    arg_parser.add_argument(
        '--api_key', help='The API key used to authenticate', default=default_api_key)
    arg_parser.add_argument(
        '--dataset-id', help='The ID of the dataset to ingest data to, overrides dataset-name')
    arg_parser.add_argument(
        '--dataset-name', help='The name of the dataset to ingest data to, fails if more than one dataset found')
    arg_parser.add_argument(
        '--entity-count', help='The number of entities to generate in the test pattern', default=64)

    args = arg_parser.parse_args()

    if not args.api_key:
        if args.user and args.host:
            api_key = conduce.config.get_api_key(args.user, args.host)
            if api_key:
                args.api_key = api_key

    kind = 'generated-test-dot'

    if args.dataset_id is not None:
        if check_dataset_id(args):
            dataset_id = args.dataset_id
        else:
            print '{} is an invalid dataset ID'.format(args.dataset_id)
            sys.exit(2)
    elif args.dataset_name is not None:
        dataset_id = get_dataset_id(args)
        if dataset_id is None:
            print 'Could not find dataset named {}'.format(args.dataset_name)
            sys.exit(3)
    else:
        dataset_id = initialize_test(kind, args)

    path = os.path.abspath('../build/src/entity-generator/entity-generator')
    if not os.path.exists(path):
        print '{} does not exist'.format(path)
        print 'Please build entity-generator as documented in the README file'
        sys.exit(1)

    cmd_args = [path, '--asynchronous', '--test-pattern', '--dataset-id={}'.format(dataset_id),
                '--entity-count={}'.format(args.entity_count), '--kind={}'.format(kind)]
    if args.host:
        cmd_args.append('--host={}'.format(args.host))
    else:
        print "Target host must be specified on command line or in Conduce config."
    if args.api_key:
        cmd_args.append('--api-key={}'.format(args.api_key))
    else:
        print "An API key must be provided on the command line or through your Conduce config."
    subprocess.call(cmd_args)


if __name__ == '__main__':
    main()
