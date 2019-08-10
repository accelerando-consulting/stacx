FROM golang
RUN apt-get update && apt-get install -y build-essential python-serial
ADD cli.mk /go
RUN make -C /go -f cli.mk installcli
RUN make -C /go -f cli.mk cliconfig
RUN make -C /go -f cli.mk installcore
RUN mkdir /app
ADD Makefile /go
RUN make -C /go libs
RUN make -C /go extralibs
RUN rm /go/Makefile /go/cli.mk