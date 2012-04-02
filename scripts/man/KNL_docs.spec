Summary:            KNL manpages
Name:               KNL-docs
Source:             %{name}-%{version}.tar.gz
Version:            0.1
Release:            1%{?dist}
Group:              Documentation
License:            N/A
BuildArch:          noarch

%description
Install optionnal manpages of the KNL in the cldk manpages.

Requires:           CLDK

%prep
%setup -q -n %{name}

%clean

%build
cp -pa man %{_builddir}/

%install
rm -rf %{buildroot}/*
mkdir -p %{buildroot}/opt/cldk/
cp -pa man/ %{buildroot}/opt/cldk/
for FILE in %{buildroot}/opt/cldk/man/man9[x]/*.9[x]; do
    gzip -p $FILE
done

%files
%defattr(-,root,root,-)
%{buildroot}/opt/cldk/man/man9[x]/*

%changelog
* Thu Mar 29 2012 Jerome Chantelauze 
- Initial release
