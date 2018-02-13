Welcome to the Personalization module.

The personalization module adds highly configurable implicit and explicit 
personalization to Drupal websites based on geolocation and taxonomies. It's 
designed for larger content heavy sites but would work effectively on any. It 
comes with a "Suggested content" block and paginated listing but the intention 
is for developers to call the modules functions directly to retrieve users 
suggested content to do with as they please.

Installing the module:

You must also enable the core Taxonomy module to use Personalization

Navigate to administer >> build >> modules. Enable Personalization

GEOGRAPHICAL BASED PERSONALIZATION

There are three options:
* Default - requesting the location from the user will require them to be using
a HTML5 compliant browser and your Drupal theme to be HTML5.
* Smart IP - geo location data are loaded from Smart IP module.
* IP Geolocation Views & Maps - geo location data are loaded from IP Geolocation
Views & Maps module.

NOTE: Smart IP and IPGV&M has own tools for requesting location from user by
using HTML5 (Device Geolocation submodule of Smart IP, "Employ a free Google
service" option in DATA COLLECTION OPTIONS). If you use one fo these modules it
makes sense to use their native tool for that, unless you have specific flow.

IMPORTANT: Don't use at same time 'Default' option and enabled HTML5
functionality in Smart IP and IPGV&M for requesting location from user. Because
that will prompt user for location several times.

For more details of how to configure Smart IP and IPGV&M see their README.txt
and modules pages on drupal.org:
* Smart IP (https://drupal.org/project/smart_ip)
* IP Geolocation Views & Maps (https://drupal.org/project/ip_geoloc)
