FROM ubuntu
RUN apt-get update --fix-missing && export DEBIAN_FRONTEND=noninteractive \
     && apt-get -y install --no-install-recommends --fix-missing libpq-dev