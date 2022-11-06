# classinet

Classinet it a fully automatic machine learning inference cloud service. 
This repo contains the client side options for classinet. 
We are building support for every environment on every hardware, but this is work in progress.
If your environment of choice is not supported yet please contact us on our site https://www.classinet.com to get updates on and perhaps help us develop it faster. 

The core of the classinet client side library is provided in binary form compiled for all platforms. 
There are C API and C++ API variants of the library, as well as statically linked and dynamically linked options when applicable. 
Although the code library is proprietary and binary only copyright (c) 2022 Classinet Technologies LTD, the use cases and examples on each target platform are public domain. 

Source code on this repo can be a great demonstration of how to best use the classinet API. In particular the sources for the classinet command line utility demonstrate all the classinet API.

The secret sauce to any efficient library is asynchronous API. 
Meaning each API call results in a callback to some user provided function when the result is ready. 
Under the hood classinet is 100% asynchronous. 
But of course some applications favor simplicity over efficiency. 
The classinet API provide synchronous (serial) and well as asynchronous variants to all operations. 

We welcome user contributions of client side adaptors for other platforms. 

Currently available client side options on this repo are:

C API

C++ API

Python extension. Based on the C API source code provided. 

