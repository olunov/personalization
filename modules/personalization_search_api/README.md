# Personalization Search API

##INTRODUCTION
This module integrates the Personalization with the Search API module [1],
allowing users to track searching keywords and score mapped terms accordingly.

[1] http://www.drupal.org/project/search_api

##REQUIREMENTS
This module requires the following module(s):
* Personalization (https://www.drupal.org/project/personalization)
* Search API (http://www.drupal.org/project/search_api)

##INSTALLATION
* Install as you would normally install a contributed Drupal module. 
See: https://drupal.org/documentation/install/modules-themes/modules-7 for 
further information.
* Go to Search API configuration page (admin/config/search/search_api), find 
index you would like to track and choose operation 'Filters' from options list 
(or click edit and go 'Filters' tab).
* Check 'Personalization Search API processor' and save the index.

##MAINTAINERS
Current maintainers:
* Oleksandr Lunov ([alunyov](https://www.drupal.org/user/103985))
* Dan Ashdown ([danashdown](https://www.drupal.org/user/1225806))

Supporting organizations: 
* [EPAM Systems](https://www.drupal.org/epam-systems)
