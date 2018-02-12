# Entity generator helper scripts

These helper scripts are intended to make visualizing a "live" test pattern easy.

## Steps:

1. Run `setup.sh` to install the python virtual environment and dependencies
1. Run `./generate-test-pattern <host> <api-key>`.  This will create a dataset, lens template and start ingesting.
1. Optional:  When finished run `cleanup-test-patterns` to delete all resources with "generated-test-pattern" in their names (may be DANGEROUS).
