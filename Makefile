all: demo_file_decoding demo_stream_decoding demo_process_picture

demo_process_picture:demo_process_picture.c clist.c
	gcc -fPIC -g  demo_process_picture.c clist.c -o demo_process_picture  -lpthread -lzmq -lcjson
#	gcc -fPIC  demo_process_picture.c clist.c xml.c -o demo_process_picture  -lpthread -lzmq  

demo_stream_decoding:demo_stream_decoding.c
	g++ -fPIC -std=c++11  -I../include -L../lib demo_stream_decoding.c -o demo_stream_decoding  -lpthread -lzmq -lavformat -lavcodec -lavutil -lswscale -lswresample -lavfilter -lams_codec 


demo_file_decoding:demo_file_decoding.c
	g++ -fPIC -std=c++11  -I../include -L../lib demo_file_decoding.c -o demo_file_decoding  -lpthread -lzmq -lavformat -lavcodec -lavutil -lswscale -lswresample -lavfilter -lams_codec 

clean:
	rm -rf demo_file_decoding  demo_stream_decoding demo_process_picture 
