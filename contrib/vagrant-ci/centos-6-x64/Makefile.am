vm-check:
	vagrant up --no-provision no-deps-build && vagrant provision no-deps-build; \
      echo "Destroy the VM by running 'make destroy-vms'"

vm-check-full:
	vagrant up --no-provision full-build && vagrant provision full-build; \
      echo "Destroy the VM by running 'make destroy-vms'"

destroy-vms:
	-vagrant destroy no-deps-build
	-vagrant destroy full-build

EXTRA_DIST = Vagrantfile Makefile.in

CLEANFILES = Makefile.in
