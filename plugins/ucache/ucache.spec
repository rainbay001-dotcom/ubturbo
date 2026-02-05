%global ucache_version    1.0.1
%global release_version 2
%global os_version x1.eulerx_a2
%global release_os_version %{release_version}.%{os_version}

Name:          ubturbo-ucache
Version:       %{ucache_version}
Release:       %{release_os_version}
Summary:       Huawei UCache driver
License:       GPLv2
URL:           https://support.huawei.com
Source0:       ucache1.0.tar.gz
Provides:      %{name}
Vendor:        Huawei Technologies Co., Ltd.
BuildRoot:     %{buildroot}
ExclusiveArch: %arm64
BuildRequires: kernel-devel >= 5.10.0-136.12.0.86 make >= 4.3 gcc >= 10.3.1
Requires:      kernel >= 5.10.0-136.12.0.86
Requires:      ubs_engine

%define debug_package %{nil}
%define ubturbo_lib_dir /opt/ubturbo/lib
%define ubturbo_conf_dir /opt/ubturbo/conf

%description
This package contains the Huawei ucache Driver

%prep
%setup -q -T -b 0 -c -n ucache1.0

%build
cd %{_builddir}/ucache1.0 && bash -x build.sh install_all -c

%install
# install ubturbo
echo "########RPM_BUILD_ROOT=${RPM_BUILD_ROOT}"
mkdir -p -m755 ${RPM_BUILD_ROOT}/%{ubturbo_lib_dir}
mkdir -p -m755 ${RPM_BUILD_ROOT}/%{ubturbo_conf_dir}
%{__install} -b -m 0644 %{_builddir}/ucache1.0/cmake-build-release/lib/libucache_os_turbo_plugin.so ${RPM_BUILD_ROOT}/%{ubturbo_lib_dir}
%{__install} -b -m 0644 %{_builddir}/ucache1.0/cmake-build-release/conf/plugin_turbo_ucache.conf ${RPM_BUILD_ROOT}/%{ubturbo_conf_dir}

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%{ubturbo_lib_dir}/libucache_os_turbo_plugin.so
%{ubturbo_conf_dir}/plugin_turbo_ucache.conf
