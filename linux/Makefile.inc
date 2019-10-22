
KHL_SRC = net/connection.cpp system/fastfile.cpp net/http.cpp db/sqlite3.cpp db/postgres.cpp net/session.cpp \
	app/config.cxx app/s2h-session.cxx app/s2hb-storage.cxx app/server2hb.cxx net/compactbinary.cpp util/logger.cpp \
	util/utils.cxx net/geoip.cxx net/mail.cxx net/token.cxx 

KHL_OBJECTS = net/connection.o system/fastfile.o net/http.o db/sqlite3.o db/postgres.o net/session.o \
	app/config.o app/s2h-session.o app/s2hb-storage.o app/server2hb.o net/compactbinary.o util/logger.o \
	util/utils.o net/geoip.o net/mail.o net/token.o

KHL_INC = 

KHL_LIB = -L/usr/local/lib \
	-lz -lsqlite3 -lcurl -lpthread -lstdc++ -lcrypto -lpqxx -lpq -lm -lmaxminddb -lcpp_redis -ltacopie \
        -levent -levent_pthreads -lboost_thread -lboost_filesystem -lboost_system -lboost_regex -lboost_chrono -lboost_program_options -lboost_date_time 


KHL_EXT_SRC = json/json_reader.cpp json/json_value.cpp json/json_writer.cpp

KHL_EXT_OBJECTS = json/json_reader.o json/json_value.o json/json_writer.o

