# Two stages: compile the C++ server, then copy only the binary and the web pages into a small runtime image.
# Ubuntu is used because it ships the real MySQL client library (libmysqlclient) that the code was written against.
# The base image is pinned by digest so every build starts from the same bytes. To take newer security fixes, run
#   docker pull ubuntu:24.04 && docker inspect --format '{{index .RepoDigests 0}}' ubuntu:24.04
# and put the new digest in both FROM lines.

FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake libmysqlclient-dev libssl-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY tests tests
COPY third_party third_party
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target ims -j2

FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
RUN apt-get update && apt-get install -y --no-install-recommends libmysqlclient21 libssl3t64 ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --uid 10001 --no-create-home --shell /usr/sbin/nologin ims
WORKDIR /app
COPY --from=build /src/build/ims /app/ims
COPY public /app/public
# the app runs as a non-root user: make sure every web file is world-readable whatever its mode was on the host
RUN chmod -R a+rX /app/public
ENV PUBLIC_DIR=/app/public \
    PORT=8080 \
    COOKIE_SECURE=1
USER ims
EXPOSE 8080
CMD ["/app/ims"]
