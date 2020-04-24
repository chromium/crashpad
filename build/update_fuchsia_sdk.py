#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the Fuchsia SDK to the given revision. Should be used in a 'hooks_os'
entry so that it only runs when .gclient's target_os includes 'fuchsia'."""

import argparse
import itertools
import logging
import os
import re
import shutil
import subprocess
import sys
import tarfile

DIR_SOURCE_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))

SDK_ROOT = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'fuchsia', 'sdk')

IMAGES_ROOT = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'fuchsia', 'images')

SDK_SIGNATURE_FILE = '.lastversion'

SDK_TARBALL_PATH_TEMPLATE = (
    'gs://fuchsia/development/{sdk_version}/sdk/{platform}-amd64/gn.tar.gz')


def GetHostOsFromPlatform():
    return {'darwin': 'mac', 'linux': 'linux', 'linux2': 'linux'}[sys.platform]


def GetSdkHashForPlatform():
    version_file = os.path.join(
        os.path.dirname(__file__), 'fuchsia_sdk.version')
    with open(version_file, 'r') as f:
        for line in f.readlines():
            stripped = line.strip()
            if not stripped or stripped.startswith('#'):
                continue
            return stripped


def GetSdkTarballForPlatformAndHash(sdk_version):
    return SDK_TARBALL_PATH_TEMPLATE.format(
        sdk_version=sdk_version, platform=GetHostOsFromPlatform())


def GetSdkSignature(sdk_version, boot_images):
    return 'gn:{sdk_version}:{boot_images}:'.format(
        sdk_version=sdk_version, boot_images=boot_images)


def EnsureDirExists(path):
    if not os.path.exists(path):
        os.makedirs(path)


# Updates the modification timestamps of |path| and its contents to the
# current time.
def UpdateTimestampsRecursive():
    for root, dirs, files in os.walk(SDK_ROOT):
        for f in files:
            os.utime(os.path.join(root, f), None)
        for d in dirs:
            os.utime(os.path.join(root, d), None)


# Fetches a tarball from GCS and uncompresses it to |output_dir|.
def DownloadAndUnpackFromCloudStorage(url, output_dir):
    # Pass the compressed stream directly to 'tarfile'; don't bother writing it
    # to disk first.
    cmd = ['gsutil', 'cp', url, '-']
    logging.debug('Running "%s"', ' '.join(cmd))
    task = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        tarfile.open(
            mode='r|gz', fileobj=task.stdout).extractall(path=output_dir)
    except tarfile.ReadError:
        task.wait()
        stderr = task.stderr.read()
        raise subprocess.CalledProcessError(
            task.returncode, cmd, "Failed to read a tarfile from gsutil.py.{}".
            format(stderr if stderr else ""))
    task.wait()
    if task.returncode:
        raise subprocess.CalledProcessError(task.returncode, cmd,
                                            task.stderr.read())


def DownloadSdkBootImages(sdk_version, boot_image_names):
    if not boot_image_names:
        return

    all_device_types = ['generic', 'qemu']
    all_archs = ['x64', 'arm64']

    images_to_download = set()
    for boot_image in boot_image_names.split(','):
        components = boot_image.split('.')
        if len(components) != 2:
            continue

        device_type, arch = components
        device_images = all_device_types if device_type == '*' else [
            device_type
        ]
        arch_images = all_archs if arch == '*' else [arch]
        images_to_download.update(itertools.product(device_images, arch_images))

    for image_to_download in images_to_download:
        device_type = image_to_download[0]
        arch = image_to_download[1]
        image_output_dir = os.path.join(IMAGES_ROOT, arch, device_type)
        if os.path.exists(image_output_dir):
            continue

        logging.info('Downloading Fuchsia boot images for %s.%s...' %
                     (device_type, arch))
        images_tarball_url = \
            'gs://fuchsia/development/{sdk_version}/images/'\
            '{device_type}-{arch}.tgz'.format(
                sdk_version=sdk_version, device_type=device_type, arch=arch)
        DownloadAndUnpackFromCloudStorage(images_tarball_url, image_output_dir)


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--verbose',
        '-v',
        action='store_true',
        help='Enable debug-level logging.')
    parsed = parser.parse_args(args)

    logging.basicConfig(level=logging.DEBUG if parsed.verbose else logging.INFO)

    sdk_version = GetSdkHashForPlatform()
    if not sdk_version:
        return 1

    # Only get qemu images for now.
    boot_images = 'qemu.*'

    signature_filename = os.path.join(SDK_ROOT, SDK_SIGNATURE_FILE)
    current_signature = (open(signature_filename, 'r').read().strip()
                         if os.path.exists(signature_filename) else '')
    if current_signature != GetSdkSignature(sdk_version, boot_images):
        logging.info('Downloading GN SDK %s...' % sdk_version)

        if os.path.isdir(SDK_ROOT):
            shutil.rmtree(SDK_ROOT)

        EnsureDirExists(SDK_ROOT)
        DownloadAndUnpackFromCloudStorage(
            GetSdkTarballForPlatformAndHash(sdk_version), SDK_ROOT)

        # Clean out the boot images directory.
        if (os.path.exists(IMAGES_ROOT)):
            shutil.rmtree(IMAGES_ROOT)
            os.mkdir(IMAGES_ROOT)

        try:
            DownloadSdkBootImages(sdk_version, boot_images)
        except subprocess.CalledProcessError as e:
            logging.error(("command '%s' failed with status %d.%s"),
                          " ".join(e.cmd), e.returncode,
                          " Details: " + e.output if e.output else "")
            return 1

    with open(signature_filename, 'w') as f:
        f.write(GetSdkSignature(sdk_version, boot_images))

    UpdateTimestampsRecursive()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
