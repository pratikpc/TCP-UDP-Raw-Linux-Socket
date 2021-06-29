# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.183.0/containers/cpp/.devcontainer/base.Dockerfile

# [Choice] Debian / Ubuntu version: debian-10, debian-9, ubuntu-20.04, ubuntu-18.04
ARG VARIANT="ubuntu-20.04"
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

# [Optional] Uncomment this section to install additional packages.
RUN apt-get update --fix-missing && export DEBIAN_FRONTEND=noninteractive \
     && apt-get -y install --no-install-recommends --fix-missing ninja-build clang-format software-properties-common

RUN add-apt-repository ppa:git-core/ppa && apt-get update --fix-missing &&  export DEBIAN_FRONTEND=noninteractive \
     && apt-get -y install git