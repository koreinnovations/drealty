<?php

class drealtyResources {

  private $defaultCache = 'cache';

  function __construct() {
    drealty_load_phrets();
  }

  function GetActiveFields($conid, $resource) {
    $cache_name = "drealty_fields_active_{$resource}_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);
    if ($cached) {
      $result = db_query("SELECT * FROM {drealty_fields} WHERE conid = :conid AND resource = :resource AND display = 1", array(':conid' => $conid, ':resource' => $resource))->fetchAll();
      $fields = array();
      foreach ($result as $field) {
        $fields[$field->systemname] = $field;
      }
      cache_set($cache_name, $fields);
      return $fields;
    } else {
      return $cached->data;
    }
  }

  /**
   * Retrieve resource mappings for a given connnection
   *
   * @param int $conid
   *  The connection you want to retrieve mappings for
   * @return array
   */
  function GetResourceMappings($conid) {
    $cache_name = "drealty_mappings_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);
    if (!$cached) {
      $result = db_query("SELECT * FROM {drealty_resource_mappings} WHERE conid = :conid", array(':conid' => $conid))->fetchAll();
      $mappings = array();
      foreach ($result as $mapping) {
        $mappings[$mapping->entity_type] = $mapping;
      }
      cache_set($cache_name, $mappings);
    } else {
      $mappings = $cached->data;
    }
    return $mappings;
  }

  function FetchActiveFields($conid, $resource) {
    $cache_name = "drealty_fields_active_{$resource}_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);
    if (!$cached) {
      $result = db_query("SELECT * FROM {drealty_fields} WHERE conid = :conid AND resource = :resource AND display = 1", array(':conid' => $conid, ':resource' => $resource))->fetchAll();
      $fields = array();
      foreach ($result as $field) {
        $fields[$field->systemname] = $field;
      }
      cache_set($cache_name, $fields, $this->defaultCache);
      return $fields;
    } else {
      return $cached->data;
    }
  }

  function FetchFieldMappings($conid, $resource = NULL, $class_id = NULL) {

    $query = db_select('drealty_field_mappings', 'm')
      ->fields('m')
      ->condition('conid', $conid);

    if (isset($resource)) {
      $query->condition('resource', $resource);
    }
    if (isset($class_id)) {
      $query->condition('cid', $class_id);
    }

    $mappings = $query->execute()->fetchAllAssoc('field_name');
    foreach ($mappings as $mapping) {
      $mapping->data = unserialize($mapping->data);
    }
    return $mappings;
  }

  /**
   * Function to return drealtyRetsFeilds from the database
   * 
   * @param integer $conid
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @return drealtyRetsField[]
   */
  function FetchFields(integer $conid, drealtyRetsResource $resource = NULL, drealtyRetsClass $class = NULL) {

    $cache = &drupal_static(__FUNCTION__);

    if (!isset($cache)) {

      $query = db_select('drealty_fields', 'f')
        ->fields('f')
        ->condition('conid', $conif);

      if ($resource) {
        $query->condition('resource', $resource->systemname);
      }

      if ($class) {
        $query->condition('class', $class->systemname);
      }

      $fields = $query->execute()->fetchAllAssoc('systemname');

      if (count($fields)) {
        $cache = $fields;
      } else {

        $dc = new dRealtyConnection();

        if ($dc->connect($conid)) {
          $fields = array();

          $results = $dc->rets->GetMetadata($resource->systemname, $class->systemname);
          if ($results) {

            // make sure we don't have any stray fields for this connection / resource / class
            db_delete('drealty_fields')
              ->condition('conid', $conid)
              ->condition('class', $class)
              ->condition('resource', $resource->systemname)
              ->execute();

            foreach ($results as $field) {
              $field = (object) $field;
              $newField = new drealtyRetsField();
              $newField->conid = $conid;
              $newField->resource = $resource;
              $newField->systemname = $field->SystemName;
              $newField->standardname = $field->StandardName;
              $newField->longname = $field->LongName;
              $newField->datatype = $field->DataType;
              $newField->max_length = $field->MaximumLength;
              $newField->field_precision = $field->Precision === '' ? 0 : $field->Precision;
              $newField->interpretation = $field->Interpretation;
              $newField->class = array($class->systemname);
              $newField->lastupdate = strtotime("now");


              if (drupal_write_record('drealty_fields', $newField)) {
                $fields[$newField->systemname] = $newField;
              }
            } // end foreach
          } else {
            $this->handleRetsError($dc->rets);
          }
          $dc->disconnect();
        } // end if $dc->connect
        // set the static cache
        $cache = $fields;
      }
    }

    return $cache;
  }

  private function handleRetsError(phRETS $rets) {
    $error = $rets->Error();
    drupal_set_message($error, 'error');
  }

  /**
   * Retreive a drealtyRetsClass from the db
   * @param integer $id
   * @return drealtyRetsClass 
   */
  public function FetchClass(integer $id) {
    $cache = &drupal_static(__FUNCTION__);
    if (!isset($cache)) {
      $result = db_select('drealty_classes', 'dc')
        ->fields('dc')
        ->condition('cid', $id)
        ->range(0, 1)
        ->execute()
        ->fetchObject();

      $cache = $result;
    }
    return $cache;
  }

  /**
   *
   * @param integer $conid
   * @param drealtyRetsResource $resource
   * @return drealtyRetsClass[]
   */
  function FetchClasses(integer $conid, drealtyRetsResource $resource) {

    $cache = &drupal_static(__FUNCTION__);

    if (!isset($cache)) {
      // need to retrieve these from the db
      $classes = db_select('drealty_classes', 'dc')
        ->fields('dc')
        ->condition('conid', $conid)
        ->condition('rid', $resource->rid)
        ->execute()
        ->fetchAllAssoc('systemname');

      //check to see if we got any
      if (count($classes)) {
        // set the static cache
        $cache = $classes;
      } else {
        // we didn't find any classes so we'll need to get the metadata from the RETS provider
        $dc = new dRealtyConnection();
        if ($dc->connect($conid)) {
          $metadata = $dc->rets->GetMetadataClasses($resource->systemname);
          if ($metadata) {


            $classes = array();
            foreach ($metadata as $class) {
              $class = (object) $class;

              $newClass = new drealtyRetsClass();
              $newClass->systemname = $class->ClassName;
              $newClass->conid = $conid;
              $newClass->standardname = $class->StandardName;
              $newClass->visiblename = $class->VisibleName;
              $newClass->description = $class->Description;
              $newClass->rid = $resource->rid;

              if (db_write_record('drealty_classes', $newClass)) {
                $classes[$newClass->systemname] = $newClass;
              }
            } // end foreach
            // set the static cache
            $cache = $classes;
          } else {
            $this->handleRetsError($dc->rets);
          }
          $dc->disconnect();
        }
      }
    }
    return $cache;
  }

  /**
   * Retrieve the current resources for a given connection.
   *
   * This will retrieve the cached resource list for a given connection. If
   * no resources are found in the cache it will attempt to load them from the db,
   * finding none there it will retrieve them from the RETS Server.
   *
   * @param int $conid
   *  Connection ID. The connection to retrieve the resources for.
   * @param bool $update
   *  Passing TRUE will cause the resources to be refreshed from the RETS server.
   * @return array()
   *  Returns and array() of resources.
   *
   */
  function FetchResources($conid) {


    $cache = &drupal_static(__FUNCTION__);

    if (!isset($cache)) {
      $resources = db_select('drealty_resources', 'dr')
        ->fields('dr')
        ->condition('conid', $conid)
        ->execute()
        ->fetchAllAssoc('systemname');

      $cache = $resources;
    } else {
      // we didn't find any classes so we'll need to get the metadata from the RETS provider
      $dc = new dRealtyConnection();
      if ($dc->connect($conid)) {
        $metadata = $dc->rets->GetMetadataResources();
        if ($metadata) {

          $resources = array();
          foreach ($metadata as $class) {
            $class = (object) $class;

            $newResource = new drealtyRetsResource();
            $newResource->conid = $conid;
            $newResource->systemname = $resource->ResourceID;
            $newResource->description = $resource->Description;
            $newResource->lastupdate = strtotime($resource->ClassDate);
            $newResource->keyfield = $resource->KeyField;

            if (db_write_record('drealty_resource', $newResource)) {
              $resources[$newResource->systemname] = $newResource;
            }
          } // end foreach
          // set the static cache
          $cache = $resources;
        } else {
          $this->handleRetsError($dc->rets);
        }
        $dc->disconnect();
      }
    }
    return $cache;
  }
    

  function get_correlation_mls_system_name($correlation_name, $resource, $conid) {
    $result = db_result(db_query("SELECT systemName FROM {drealty_fields} WHERE correlation = '%s' AND resource = '%s' AND conid = %d", $correlation_name, $resource, $conid));
    if (!empty($result)) {
      return $result;
    } else {
      return FALSE;
    }
  }

  function get_correlation_field_name($correlation_name, $resource, $conid) {
    $result = db_result(db_query("SELECT cck_field_name FROM {drealty_fields} WHERE correlation = '%s' AND resource = '%s' AND conid = %d", $correlation_name, $resource, $conid));
    if (!empty($result)) {
      return $result;
    } else {
      return FALSE;
    }
  }

  function RemoveFields($conid) {

    db_delete("drealty_fields")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_fields_", $this->defaultCache, TRUE);
  }

  function RemoveClasses($conid) {
    db_delete("drealty_classes")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_classes_", $this->defaultCache, TRUE);
  }

  function RemoveResources($conid) {
    db_delete("drealty_resources")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_resources_", $this->defaultCache, TRUE);
  }

  function RemoveFieldMappings($conid) {
    db_delete("drealty_field_mappings")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_field_mapping_", $this->defaultCache, TRUE);
  }

  function RemoveResourceMappings($conid) {
    db_delete("drealty_resource_mappings")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_mappings_", $this->defaultCache, TRUE);
  }

}

class drealtyRetsField {

  public $fid;
  public $conid;
  public $resource;
  public $systemname;
  public $standardname;
  public $longname;
  public $datatype;
  public $max_length;
  public $field_precision;
  public $interpretation;
  public $lookupname;
  public $class;
  public $rets_returned;
  public $lastupdate;

}

class drealtyRetsClass {

  public $cid;
  public $conid;
  public $systemname;
  public $standardname;
  public $visiblename;
  public $description;
  public $rid;
  public $bundle;
  public $lifetime = 28800;
  public $enabled = FALSE;
  public $configured = FALSE;
  public $lastupdate = NULL;
  public $status_values;
  public $process_images = FALSE;
  public $image_field_name;
  public $image_dir;
  public $object_type;
  public $image_chunk_size = 25;
  public $query_type = 0;
  public $chunk_size = 500;
  public $override_status_query_text;
  public $do_geocoding = FALSE;
  public $geocoder_handler;
  public $offset_field;
  public $offset_amount;
  public $offset_max;

}

class drealtyRetsResource {

  public $rid;
  public $conid;
  public $systemname;
  public $description;
  public $lastupdate;
  public $selection_values;
  public $keyfield;
  public $chunk_size;

}

class drealtyFieldMapping {

  public $conid;
  public $field_name;
  public $resource;
  public $systemname;
  public $field_api_type;
  public $data;
  public $cid;

}

class drealtyResourceMapping {

  public $mid;
  public $conid;
  public $resource;
  public $entity_type;

}