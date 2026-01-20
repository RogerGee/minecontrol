# Dockerfile - minecontrol
#
# Use this image as a base to create your own Docker images running
# minecontrol. You will need to add a Java JRE and Minecraft server jars to your
# image.

FROM debian:bullseye-slim AS base

RUN apt update \
    && apt install --force-yes -y libncurses6 libreadline8 openssl \
    && apt clean \
    && rm -rf /var/lib/apt/lists/*

FROM base AS build

RUN apt update \
    && apt install --force-yes -y \
        autoconf \
        build-essential \
        libncurses-dev \
        libreadline-dev \
        libssl-dev

ADD https://www.rserver.us/downloads/software/rlibrary/rlibrary-0.2.0.tar.gz /build/
RUN cd /build \
    && tar xf rlibrary-0.2.0.tar.gz \
    && cd /build/rlibrary-0.2.0 \
    && make -j $(( $(nproc) / 2 )) \
    && make install

ADD /configure.ac /Makefile.am /build/
ADD /*.cpp /*.h /*.tcc /build/
WORKDIR /build
RUN autoreconf -i . \
  && ./configure

RUN make -j $(( $(nproc) / 2)) \
    && make DESTDIR=/build install

FROM base
COPY --from=build /build/usr/local/ /usr/local/

RUN mkdir /etc/minecontrold/

ENTRYPOINT ["/usr/local/sbin/minecontrold"]
CMD ["--no-daemon"]
