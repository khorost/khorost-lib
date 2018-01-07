
KHL_SRC = net/connection.cpp system/fastfile.cpp net/http.cpp db/sqlite3.cpp db/postgres.cpp db/mysql.cpp net/session.cpp \
	app/config.cxx app/s2h-session.cxx app/s2hb-storage.cxx app/server2hb.cxx net/compactbinary.cpp util/logger.cpp \
	util/utils.cxx net/geoip.cxx

KHL_OBJECTS = net/connection.o system/fastfile.o net/http.o db/sqlite3.o db/postgres.o db/mysql.o net/session.o \
	app/config.o app/s2h-session.o app/s2hb-storage.o app/server2hb.o net/compactbinary.o util/logger.o \
	util/utils.o net/geoip.o

KHL_INC =  -I/usr/include/mysql 

KHL_LIB = -L/usr/local/lib -lmysqlclient -lGeoIP \
	-lz -levent -levent_pthreads -lboost_thread -lboost_filesystem \
	-lboost_system -llog4cxx -lsqlite3 -lcurl -lboost_regex -lpthread \
	-lstdc++ -lcrypto -lpqxx -lpq  -lboost_program_options -lboost_date_time -lm


KHL_EXT_SRC = json/jsoncpp.cpp g3log/time.cpp g3log/logworker.cpp g3log/logmessage.cpp g3log/loglevels.cpp \
	g3log/logcapture.cpp g3log/g3log.cpp g3log/filesink.cpp g3log/crashhandler_unix.cpp

KHL_EXT_OBJECTS = json/jsoncpp.o g3log/time.o g3log/logworker.o g3log/logmessage.o g3log/loglevels.o \
	g3log/logcapture.o g3log/g3log.o g3log/filesink.o g3log/crashhandler_unix.o

