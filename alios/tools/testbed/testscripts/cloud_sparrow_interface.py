# -*- coding: utf-8 -*-
'''
Created on 2017-4-18

@author: kaiyu.wky
'''

import os, sys, json
import traceback, StringIO
import urllib, importlib
import zipfile, tarfile
import time, base64, shutil
import xml.dom.minidom

imagePath = ''
ifDeleteImage = True

lastDownloadPercentDisplay = None

TASK_RESULT_SUCCESS = 0
TASK_RESULT_FAIL = 1000
TASK_RESULT_ERROR_DEVALLOC = 1001
TASK_RESULT_ERROR_IMGDOWNLOAD = 1002
TASK_RESULT_ERROR_IMGPROGRAM = 1003
TASK_RESULT_ERROR_MESHCONNECT = 1004

def log_info(msg):
    print msg

def log_error(msg):
    sys.stderr.write(str(msg) + '\n')

def optParser():
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option('--username', dest='username', help='user name')
    parser.add_option('--url', dest='url', help='server url')
    parser.add_option('--serial_number', dest='sn', help='device serial number')
    parser.add_option('--device_name', dest='device_name', help='device name')
    parser.add_option('--imei', dest='imei', help='device imei')
    parser.add_option('--device_version', dest='device_version', help='device version')
    parser.add_option('--adb', dest='adb', default='adb', help='adb path')
    parser.add_option('--XML', dest='xml', action='store_true', default=True, help='parse xml flag')
    parser.add_option('--producttypeid', dest='producttypeid', help='product type id')
    parser.add_option('--task_execution_id', dest='task_execution_id', help='task exec id')
    parser.add_option('--producttag', dest='product_tag', help='product tag')
    parser.add_option('--productname', dest='product_name', help='product name')

    #Alios Things specific options
    parser.add_option('--testname', dest='testname', help='test name')
    parser.add_option('--device', dest='devicepath', help='device path')
    parser.add_option('--firmware', dest='firmware', help='firmware path')
    parser.add_option('--caseid', dest='caseid', help='alink case id')
    parser.add_option('--userid', dest='userid', help='alink user id')
    parser.add_option('--server', dest='server', help='server ip')
    parser.add_option('--port', dest='port', help='server port')
    parser.add_option('--wifissid', dest='wifissid', help='wifi ssid')
    parser.add_option('--wifipass', dest='wifipass', help='wifi password')
    parser.add_option('--debug', dest='debug', help='debug option')

    return parser

def getXmlConfig(filePath, tag, name=None, attribute=None):
    if not os.path.isfile(filePath):
        log_info(filePath + " doesn't exist")
    document = xml.dom.minidom.parse(filePath)
    rootElement = document.documentElement
    if name:
        elements = rootElement.getElementsByTagName(tag)
        for element in elements:
            if element.getAttribute("sname") == name:
                if attribute:
                    return element.getAttribute(attribute)
                else:
                    return element
    else:
        return rootElement.getElementsByTagName(tag)


def get_param(key):
    value = getXmlConfig(os.path.basename(__file__)[0:-3] + '.xml', 'test-parameter', key, 'default-value')
    if value is not None:
        value = value.encode('utf-8')
    return value


def getWorkDir():
    return os.path.dirname(os.path.abspath(__file__))


def exit(code, message):
    if code != 0:
        log_error(message)

    result = {}
    result['code'] = code
    result['message'] = message

    result['skip_count'] = 0
    if code == 0:
        result['pass_count'] = 1
        result['fail_count'] = 0
        result['skip_count'] = 0
    else:
        result['pass_count'] = 0
        result['fail_count'] = 1
        result['skip_count'] = 0

    resultFile = open('result.json', 'w')
    resultFile.write(json.dumps(result, sort_keys=True))
    resultFile.close()
    if os.path.exists(imagePath):
        shutil.rmtree(imagePath)
    quit()

def downloadFile(src, dest):
    try:
        if src.startswith('http') is True:
            urllib.urlretrieve(src, dest)
            log_info('Download {0} complete, save as {1}'.format(src, dest))
        elif src.startswith('//') is True:
            shutil.copy(src, dest)
        else:
            exit(TASK_RESULT_ERROR_IMGDOWNLOAD, 'Not support to download file ' + src)
    except:
        fp = StringIO.StringIO()
        traceback.print_exc(file=fp)
        log_error(fp.getvalue())
        exit(TASK_RESULT_ERROR_IMGDOWNLOAD, 'Download failed. Please check if there is enough space on computer and the image url is correct.')

def untar(fname, dirs):
    t = tarfile.open(fname)
    t.extractall(path=dirs)


def unzip_file(zipfilename, unziptodir):
    zfobj = zipfile.ZipFile(zipfilename)
    zfobj.extractall(unziptodir)

if __name__ == '__main__':
    #flush to output immediately
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
    sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', 0)

    isLocalFile = False
    userImageUrl = None
    imageUrl = None
    testFile = None
    parser = optParser()
    (options, args) = parser.parse_args()
    if options.xml:
        imageUrl = get_param('path')
        userImageUrl = get_param('userpath')
        ifDeleteImageConfig = get_param('ifDeleteImage')
        testFile = get_param('testfile')
        if ifDeleteImageConfig is not None:
            ifDeleteImage = (ifDeleteImageConfig.lower() == 'true')
    if testFile == None:
        exit(TASK_RESULT_FAIL, "test file not specified")
    if os.path.isfile(testFile+'.py') == False:
        exit(TASK_RESULT_FAIL, "test file '{0}' none exist".format(testFile))
    try:
        test = importlib.import_module(testFile)
    except:
        exit(TASK_RESULT_FAIL, "import test {0} failed".format(testFile))

    try:
        userImageUrl = userImageUrl.strip()
        if len(userImageUrl) > 0:
            try:
                imageUrl = userImageUrl.encode('utf8')
            except:
                imageUrl = userImageUrl

        log_info("imagePath parameter: " + imageUrl)
        if os.path.exists(imageUrl):
            isLocalFile = True
        else:
            if imageUrl.startswith('scm,'):
                device = '*'
                if helper.adb.Host(serial).isConnected() is True:
                    device = helper.adb.Host(serial).getDevice()
                elif helper.Adb(serial).isConnected() is True:
                    device = helper.Adb(serial).getDevice()
                log_info('get image url for device {0}'.format(device))
                imageUrl = helper.ImagePathHelper(device).getImageUrl(imageUrl.split(','))
                if imageUrl is None:
                    exit(TASK_RESULT_FAIL, 'Not valid imagePath')

        imagePath = os.path.join(getWorkDir(), 'image')
        if os.path.exists(imagePath) is False:
            log_info('make dir {0}'.format(imagePath))
            os.mkdir(imagePath)

        zipType = 'zip'
        zipImagePath = os.path.join(getWorkDir(), 'flash_image.zip')
        if imageUrl[-7:] == '.tar.gz':
            zipType = 'tar.gz'
            zipImagePath = os.path.join(getWorkDir(), 'flash_image.tar.gz')
        elif imageUrl[-4:] == '.bin':
            zipType = 'bin'
            zipImagePath = os.path.join(getWorkDir(), 'flash_image.bin')

        if isLocalFile is False:
            downloadFile(imageUrl, zipImagePath)
        else:
            log_info('copy local image file: ' + imageUrl + '...')
            shutil.copyfile(imageUrl, zipImagePath)

        log_info('Extracting {0} to {1}'.format(zipImagePath, imagePath))
        pacPath = os.path.join(imagePath, 'image.bin')
        if zipType == 'bin':
            if isLocalFile is True:
                shutil.move(zipImagePath, pacPath)
            else:
                shutil.copyfile(zipImagePath, pacPath)
        else:
            if zipType == 'tar.gz':
                untar(zipImagePath, imagePath)
            else:
                unzip_file(zipImagePath, imagePath)
            subs = os.listdir(imagePath)
            destImagePath = imagePath
            if len(subs) == 1 and os.path.isdir(os.path.join(imagePath, subs[0])):
                destImagePath = os.path.join(destImagePath, subs[0])
            for subfile in os.listdir(destImagePath):
                if subfile[-4:] == '.bin':
                    shutil.move(os.path.join(destImagePath, subfile), pacPath)
                    break
        if ifDeleteImage and os.path.exists(zipImagePath):
            log_info('remove {0}'.format(zipImagePath))
            os.remove(zipImagePath)

        [code, msg] = test.main(filename=pacPath)
        if code != 0:
            code = TASK_RESULT_FAIL
        exit(code, msg)
    except SystemExit:
        quit()
    except:
        fp = StringIO.StringIO()
        traceback.print_exc(file=fp)
        log_error(fp.getvalue())
        exit(TASK_RESULT_FAIL, fp.getvalue())
