## VAWebsite

VAWebsite is the project for the official static web presentation of VA (Virtual Acoustics).

You can browse it following the domains:

 - **[virtualacoustics.org](http://www.virtualacoustics.org) (official domain)**
 
 - [virtualacoustics.de](http://www.virtualacoustics.de) *(German domain, redirects to the official website)*

### Build with MkDocs

If you want to build the website using MkDocs you require some packages which can be installed via pip.
For this, the [requirements.txt](requirements.txt) is provided.
You can run

``` cmd
py -m pip install -r requirements.txt
```

or

``` cmd
pip install -r requirements.txt
```

to install MkDocs and all its dependencies/plug-ins.

Then, to build the website with MkDocs run the following command:

``` cmd
mkdocs build
```

In order to test the website run

``` cmd
mkdocs serve
```

if you want to develop the theme add the `--watch-theme` option.



### Legal notice

The Institute for Hearing Technology and Acoustics (IHTA) at RWTH Aachen University is responsible for the contents of this website.


### License

Copyright 2015-2022 Institute for Hearing Technology and Acoustics (IHTA), RWTH Aachen University.

Creative Commons Attribution 4.0 (CC BY 4.0)
https://creativecommons.org/licenses/by/4.0/


### Template

VAWebsite uses the following HTML template:

Landed by HTML5 UP
html5up.net | @ajlkn
Free for personal and commercial use under the CCA 3.0 license (html5up.net/license)

#### Credits

Demo Images:
 - Unsplash (unsplash.com)

Icons:
 - Font Awesome (fontawesome.github.com/Font-Awesome)

Other:
 - jQuery (jquery.com)
 - html5shiv.js (@afarkas @jdalton @jon_neal @rem)
 - CSS3 Pie (css3pie.com)
 - Respond.js (j.mp/respondjs)
 - Skel (skel.io)