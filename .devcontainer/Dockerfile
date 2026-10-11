FROM ubuntu:24.04@sha256:c4a8d5503dfb2a3eb8ab5f807da5bc69a85730fb49b5cfca2330194ebcc41c7b AS base

ENV ZSDK_VERSION="1.0.1"
ARG ZEPHYR_SDK_DIR="/opt"
ENV ZEPHYR_SDK=${ZEPHYR_SDK_DIR}/zephyr-sdk-${ZSDK_VERSION}
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

RUN apt-get -y update && \
	apt-get --no-install-recommends -y install \
		ccache \
		cmake \
		device-tree-compiler \
		dfu-util \
		file \
		g++-multilib \
		gcc \
		gcc-multilib \
		git \
		gperf \
		libmagic1 \
		libsdl2-dev \
		make \
		ninja-build \
		python3-dev \
		python3-pip \
		python3-setuptools \
		python3-tk \
		python3-wheel \
		wget \
		xz-utils \
		&& \
	apt-get clean -y && \
	apt-get autoremove --purge -y && \
	rm -rf /var/lib/apt/lists/*


FROM base AS step-sdk

WORKDIR "${ZEPHYR_SDK_DIR}"
ADD https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZSDK_VERSION}/zephyr-sdk-${ZSDK_VERSION}_linux-x86_64_minimal.tar.xz zephyr-sdk.tar.xz
RUN tar xf zephyr-sdk.tar.xz
WORKDIR "${ZEPHYR_SDK}"
RUN ./setup.sh -t arm-zephyr-eabi -t riscv64-zephyr-elf -h





FROM base AS final
ARG UID=1000
ARG GID=1000
ARG USERNAME="ubuntu"
ENV ZEPHYR_BASE=/home/${USERNAME}/workspace/zephyr
ENV PATH="${ZEPHYR_BASE}/scripts:${PATH}"

RUN apt-get -y update && \
	apt-get --no-install-recommends -y install \
		build-essential \
		ca-certificates \
		clang-tidy \
		debhelper \
		gdb \
		libfontconfig1 \
		libfreetype6 \
		libglib2.0-0 \
		libice6 \
		libsm6 \
		libusb-1.0-0-dev \
		libx11-6 \
		libx11-xcb1 \
		libxcb-icccm4 \
		libxcb-image0 \
		libxcb-keysyms1 \
		libxcb-randr0 \
		libxcb-render-util0 \
		libxcb-render0 \
		libxcb-shape0 \
		libxcb-shm0 \
		libxcb-sync1 \
		libxcb-xfixes0 \
		libxcb1 \
		libxext6 \
		libxkbcommon-x11-0 \
		libxkbcommon0 \
		libxrender1 \
		locales \
		picocom \
		udev \
		usbutils \
		wget \
		&& \
	apt-get clean -y && \
	apt-get autoremove --purge -y && \
	rm -rf /var/lib/apt/lists/*

# Initialise system locale
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8
RUN locale-gen $LANG

# Install Python dependencies
RUN	pip3 install --no-cache-dir --break-system-packages \
		-r https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/scripts/requirements.txt \
		-r https://raw.githubusercontent.com/zephyrproject-rtos/mcuboot/main/scripts/requirements.txt \
		GitPython imgtool junitparser junit2html numpy protobuf PyGithub \
		pylint sh statistics west \
		nrf-regtool~=7.0.0 jsonschema

# Add zephyr SDK
COPY --from=step-sdk /opt/zephyr-sdk-${ZSDK_VERSION} /opt/zephyr-sdk-${ZSDK_VERSION}

# Install Jlink
RUN wget -q --post-data "accept_license_agreement=accepted" https://www.segger.com/downloads/jlink/JLink_Linux_x86_64.deb && \
	/lib/systemd/systemd-udevd -d && \
	dpkg -i ./JLink_Linux_x86_64.deb && \
	rm ./JLink_Linux_x86_64.deb

# Install Ozone
RUN wget -q https://www.segger.com/downloads/jlink/Ozone_Linux_x86_64.deb && \
	dpkg -i ./Ozone_Linux_x86_64.deb && \
	rm ./Ozone_Linux_x86_64.deb

# Install latest pyocd rules
ADD https://github.com/pyocd/pyOCD/raw/refs/heads/main/udev/49-stlinkv3.rules /etc/udev/rules.d/49-stlinkv3.rules
ADD --chmod=+x https://files.nordicsemi.com/ui/api/v1/download?repoKey=swtools&path=external/nrfutil/executables/x86_64-unknown-linux-gnu/nrfutil&isNativeBrowsing=false /usr/local/bin/nrfutil

USER ${USERNAME}
WORKDIR /home/${USERNAME}/workspace