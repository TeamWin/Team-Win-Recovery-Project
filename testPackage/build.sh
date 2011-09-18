#!/bin/sh

cd res
zip -r ../update/META-INF/teamwin/twrp.zip *
cd ..

cd update
zip -r ../update-unsigned.zip *
cd ..
java -classpath testsign.jar testsign update-unsigned.zip update.zip
rm update-unsigned.zip

