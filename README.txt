/* $Id$ */

Description
----------------------------------

This module allows for the importing and display of MLS Listing Data from a RETS server. (The
RETS specification can be found at (http://www.rets.org).

The module provides 4 new content types to facilitate the display of the MLS Data:

* Properties
* Agents
* Offices
* Open Houses


Requirements & Required Modules
----------------------------------

php requirements:
* php 5.2.x - http://www.php.net
* php cURL support - http://www.php.net/manual/en/curl.setup.php

module requirements:
* Content Construction Kit (CCK) - http://drupal.org/project/cck
  * Field Group - part of CCK
  * Text - part of CCK
  * Number - part of CCK
  * Option Widgets - part of CCK
  * Imagefield - (http://drupal.org/project/imagefield
* Date - (http://drupal.org/project/date
* Imagecache - (http://drupal.org/project/imagecache


Recomended Modules
----------------------------------
* Location - http://drupal.org/project/location
* GMap - http://drupal.org/project/gmap
* Apachesolr - http://drupal.org/project/apachesolr
* Content Permissions - part of CCK
* Permissions API - http://drupal.org/project/permissions_api


Installation
-----------------------------------

* To install, follow the general directions available at:
http://drupal.org/getting-started/5/install-contrib/modules

You will need access to a RETS server, this is typically setup and provided by
your MLS providor. They will need to provide you with a username, password,
login/service URL.


Configuration
----------------------------------

Once you have the needed credentials and login url you can proceed to admin/drealty/connection
and click to add a connection:

I. Setup a Connection:
  a. Click on 'Add Connection' and fill in the the form with the following information:

    Connection Name:
      An arbitrary connection name used to identify the connection within the system.
    Login URL:
      The login URL provided to you by your MLS.
    Username:
      The user name provided to you by your MLS.
    Password:
      The password provided to you by your MLS.
    User Agent String:
      Unless provided by your MLS you can leave this at the default setting.
    RETS Version:
      Defaults to 1.5, shouldn't need to be changed unless you are unable to connect.
    Force Basic Authentication:
      Forces HTTP Basic authentication when True for buggy IIS servers. Default is False.
    Use Compression:
      Enables GZIP compression if True. Default is False.

  b. Click 'Save'

II. Activate the Connection:

  a. Find the connection you just created in the list at admin/drealty/connection
     and click on the 'Activate' link.
  b. If the server connects you will be able to activate the connection if not
     you may need to modify some of the connection information from Step I.
  c. Once the Server has connected you will then be able to activate the connection.

III. Configuration

  After the connection has been activated you will now see several configuration
  and management options.

  a. Edit - Allows you to edit the underlying connection.
  b. Delete - Will delete the corresponding connection and remove all data associated
     with it. Caution when using this, as it will remove all configurations that have
     been associated with this connection.
  d. Deactivate - This will deactivate the connection causing it to not be updated
     during cron runs.
  e. Export - Export a connection and it's various configuration settings for import at
     a later date or into a new system.
  f. Data Management - An interface to allow the manual updating and deletion of MLS Data
     that has been imported into the system.
  g. Properties - The configuration options for the Properties/Listings to import from the
     MLS / RETS Server.
  h. Open Houses - The configuration options for the Open Houses to import from the
     MLS / RETS Server.
  i. Offices and Agents - The configuration options for the Offices and Agents to
     import from the MLS / RETS Server.

  At this time you should now configure (at a minimum) the Properties, this will
  set the system up to start importing MLS Listing information.

  a. Click on Properties.
  b. The first tab in the configuration screen allows you to select which Resource
     the listing information resides in on the RETS Server. Select the Resource that
     represents the Property data from the drop down (Typically this is "Property")
     and Click on 'Select'.
  c. The system will retrieve all the classes for the selected resource. Once
     the data is presented enable any (or all) of the classes you wish to import,
     also selecting how often to fetch this data by selecting a time period from
     the "Lifetime" drop down.
  d. Click 'Save Changes' to save the configruation.
  e. On the tabs at the top Click 'Field Selection'.
  f. The data presented here are all of the fields that are available from the
     RETS server. The columns are as follows:
      i.    Import this Field - Marking this field as 'enabled' will mark it as a field
            that you would like to include in your feed. Fields that are not enabled will
            not be included in your feed and will therefore not show up in the property
            node that is created upon import.
      ii.   CCK System Name - This is the name of the cck field that is created to
            represent the data from the MLS field in the Property Node. By default
            it's name is derived from the LongName field of the RETS server. You
            may or may not want to rename this field to something more meaningful,
            so that if you want to work with the field in Themes or Views it makes
            a bit more sense than the default.
      iii.  CCK Label - The Label to give the CCK field. Defaults to the RETS LongName
      iv.   Map to Existing CCK Field - In the case of multipule connections use
            this drop down to map incoming fields to CCK fields that were defined
            from a previous connection. Otherwise this should be left at it's default.

      ** Note **

      You'll want to make sure that you enable several 'special fields' that are needed
      in the next configuration section.

      MLS ID number: You'll want to enable the MLS ID | Listing ID | MLS nuber as it is
      used as the id field for the system.

      Remarks or Description: This MLS field will be used as the node 'Body'.

      Sale Status: Listing Status is used to determine which listings to import
      from the RETS/MLS server.

      Agent ID: Used to help determine which listings belong to a particular agent.

      Office ID: Used to determine which listings belong to a particular office.

      Price: This is the price of the property

      Picture Count: Ensure that you enable this field if you intend to import
      property images.

      Address Information: Ensure that you select fields corresponding to the
      property address. Typically these are broken down into:

        * Street Number
        * Street Direction
        * Street Name
        * City
        * State
        * Zip

     Make sure to select all that apply. You can however go back and enable ones
     that you missed or forgot.

  g. Click 'Save Configuration'. This will take a few moments as it is creating
     all the cck fields and lookup values that you have selected to import.
  h. Once the Configuration has been saved you will now have to provide some cor-
     relations.
  i. Click on 'Field Correlation' at from the tabs at the top.
  j. This screen will present all of the fields that you selected to import from
     the previous configuration screen.
  k. From the Correlation drop down map the enabled MLS fields to the
     corresponding dRealty correlations.
  l. Additionally, if you have apachesolr enabled, you can select which fields
     you would like to be indexed from this screen. You can also mark any fields
     that you would like to return in the apachesolr result set.
  m. Click 'Save Configuration' and proceed to the 'Resource Limits' section.

  ** The Resource Limits Section is the final step in the configuration for each node
  ** type you elect to enable. (Properties | Open Houses | Offices | Agents).

  n. From this screen select the values from the select list to limit the import
     query to. For properties you'll typically want to select 'Active' or 'Active' and
     'New'. Each MLS is different, so select the one(s) that are appropriate for you.
  o. Record Chunk Size. The default is 500

      *********
      When processing incoming records, the system will work with data structures
      containing this many records simultaneously, and cache the rest for later. If
      updating listings is often failing due to out of memory errors and increasing
      PHP's memory allocation is not an option, try decreasing this value. However,
      decreasing this value increases the amount of time it will take to update listings.
      The RETS server may override this value (MAXROWS).
      ********
   p. Click 'Save Configuration'.
   q. Navigate back to admin/drealty/connection and then select 'Data Management'.
   r. From here you can manually import your listings. (THIS IS RECOMMENDED)
   s. Go through and click 'Update' on each of the classes that you have setup and do
      the initial import of the data. Depending on the amount of data that you
      are importing this could take a considerable amount of time. Once the initial
      import has been completed, updates to the data are done incrementally, meaning
      you won't have to download 'ALL' the data again, as it will update anything
      that has changed since the last update has run.

   IMPORTANT

   Imports run off of cron, it is VERY important that cron is setup correctly. Cron will also need
   to run at an interval equal to or less than the shortest time period you selected to update
   you're listing data. So if you selected 1 hour as the lifetime of any of the classes
   you would need to setup cron to run at least once and hour. Otherwise, your imports
   will not run.

   Information about how to setup cron can be found here:
   
   http://drupal.org/cron
