Summary:            KNL manpages
Name:               KNL_docs
Source:             %{name}-%{version}.tar.gz
Version:            0.1
Release:            1%{?dist}
Group:              Documentation
License:            N/A
BuildArch:          noarch

BuildRoot:          %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Install optionnal manpages of the KNL in the cldk manpages.

Requires:           CLDK

%prep
%setup -q -n %{name}

%clean

%build

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/opt/cldk/
cp -ap man %{buildroot}/opt/cldk/

%files
%defattr(-,root,root,-)
/opt/cldk/man/*

%changelog
* Thu Mar 29 2012 Jerome Chantelauze 
- Initial release
