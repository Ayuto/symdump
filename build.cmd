@path C:\Python34;%path%;C:\MinGW\bin
set PYTHONHOME=C:\Python34
set PATH=%PATH%;C:\Python34
set LIB=%LIB%;C:\Python34\Lib
python setup.py build_ext -c mingw32 --inplace
@echo =================================================================
@pause