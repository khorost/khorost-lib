
KHL_SRC = net/connection.cpp system/fastfile.cpp net/http.cpp db/sqlite3.cpp db/postgres.cpp db/mysql.cpp net/session.cpp \
	app/config.cxx app/s2h-session.cxx app/s2hb-storage.cxx app/server2hb.cxx net/compactbinary.cpp util/logger.cpp \
	util/utils.cxx net/geoip.cxx net/mail.cxx net/token.cxx 

KHL_OBJECTS = net/connection.o system/fastfile.o net/http.o db/sqlite3.o db/postgres.o db/mysql.o net/session.o \
	app/config.o app/s2h-session.o app/s2hb-storage.o app/server2hb.o net/compactbinary.o util/logger.o \
	util/utils.o net/geoip.o net/mail.o net/token.o

KHL_INC =  -I/usr/include/mysql 

KHL_LIB = -L/usr/local/lib -lmysqlclient \
	-lz -levent -levent_pthreads -lboost_thread -lboost_filesystem \
	-lboost_system -llog4cxx -lsqlite3 -lcurl -lboost_regex -lpthread \
	-lstdc++ -lcrypto -lpqxx -lpq  -lboost_program_options -lboost_date_time -lm -lmaxminddb -lboost_chrono


KHL_EXT_SRC = json/json_reader.cpp json/json_value.cpp json/json_writer.cpp

KHL_EXT_OBJECTS = json/json_reader.o json/json_value.o json/json_writer.o

