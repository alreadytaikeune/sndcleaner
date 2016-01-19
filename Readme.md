Install all libraries listed in INSTALL and compile with make.

Usage:
- open_stream should be called on an SndCleaner object before being able to read the stream
- standard usage: launch a dumping thread calling sc_dump_frames and start reading the ringbuffer from somewhere else.
	sc_dump_frames will ensure the fullness of the ringbuffer while there are packets available by decoding them and filling the ringbuffer.
	Where the ringbuffer is full, it will wait for the data_available signal. Your read function when performing a read should signal
	availability for writing by calling this signal, otherwise, the write will block indefinitely. 
	

