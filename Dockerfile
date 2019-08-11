FROM golang
RUN apt-get update && apt-get install -y build-essential python-serial
ADD cli.mk /go
RUN make -C /go -f cli.mk installcli
RUN make -C /go -f cli.mk cliconfig
RUN make -C /go -f cli.mk installcore
RUN mkdir /app
ADD Makefile cli.mk /app/
RUN make -C /app libs
RUN make -C /app extralibs
RUN rm /go/cli.mk /app/cli.mk /app/Makefile
