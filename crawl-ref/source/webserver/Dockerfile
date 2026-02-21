FROM ubuntu:20.04

# Latest packages
RUN apt-get update

# Avoids getting an interactive "which timezome" prompt further down the line
# which will hang your docker build
RUN apt install tzdata -y
ENV TZ="America/New_York"

# Dependencies we didn't even know were dependencies
RUN apt install -y git

# Fix "detected dubious ownership" from git sometimes when trying
# to build (mixed ownership because filesystem is a mix of host machine
# and virtual environment)
RUN git config --global --add safe.directory /crawl

# Main build dependencies
RUN apt install -y build-essential libncursesw5-dev bison flex liblua5.1-0-dev \
    libsqlite3-dev libz-dev pkg-config python3-yaml binutils-gold python-is-python3

# Tiles build dependencies (not all technically needed for webtiles probably
# but it's nice to have options)
RUN apt install -y libsdl2-image-dev libsdl2-mixer-dev libsdl2-dev \
    libfreetype6-dev libpng-dev fonts-dejavu-core advancecomp pngcrush

# Python3
RUN apt install -y python3 pip

# And dependencies for webserver
COPY ./requirements/dev.py3.txt .
RUN pip install -r dev.py3.txt

# Make our scripts available inside the container
COPY docker/* .

# 8080 should be open
EXPOSE 8080
