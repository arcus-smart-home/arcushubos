#
# Copyright 2019 Arcus Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Hub v2 support - TI AM335x
#

hubv2:
	(export BDIR=$(PWD)/build-ti; cd poky; . ./oe-init-build-env; \
	bitbake core-image-minimal-iris || exit 1; \
	tools/create_update_file core-image-minimal-iris; \
	tools/create_update_file_bootloader core-image-minimal-iris)

hubv2-dev:
	(export BDIR=$(PWD)/build-ti; cd poky; . ./oe-init-build-env; \
	bitbake core-image-minimal-iris-dev || exit 1; \
	tools/create_update_file core-image-minimal-iris-dev; \
	tools/create_update_file_bootloader core-image-minimal-iris-dev)

hubv2-clean:
	(cd build-ti; rm -rf tmp; rm -rf cache; rm -rf sstate-cache)

hubv2-distclean: hubv2-clean
	(cd build-ti; rm -rf downloads; rm -f bitbake.lock; rm -rf recipes)

#
# Hub v3 support - NXP i.MX6DualLite
#

hubv3:
	(export BDIR=$(PWD)/build-fsl; cd poky; . ./oe-init-build-env; \
	bitbake core-image-minimal-iris || exit 1; \
	tools/create_update_file core-image-minimal-iris; \
	tools/create_update_file_bootloader core-image-minimal-iris)

hubv3-dev:
	(export BDIR=$(PWD)/build-fsl; cd poky; . ./oe-init-build-env; \
	bitbake core-image-minimal-iris-dev || exit 1; \
	tools/create_update_file core-image-minimal-iris-dev; \
	tools/create_update_file_bootloader core-image-minimal-iris-dev)

hubv3-clean:
	(cd build-fsl; rm -rf tmp; rm -rf cache; rm -rf sstate-cache)

hubv3-distclean: hubv3-clean
	(cd build-fsl; rm -rf downloads; rm -f bitbake.lock; rm -rf recipes)

