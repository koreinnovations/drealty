<?php
/**
 * @file
 * dRealty RETS metadata classes.
 */

class drealtyMetaData {

  private $defaultCache = 'cache';

  function __construct() {
    drealty_load_phrets();
  }

  /**
   * Internal Function to log errors.
   * @param phRETS $rets 
   */
  private function handleRetsError(phRETS $rets) {
    $error = $rets->Error();
    drupal_set_message(t("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text'])), 'error');
  }

  /**
   *
   * @param integer $conid
   * @param drealtyRetsResource $resource
   * @return drealtyRetsField[] 
   */
  function GetActiveFields($conid, $resource) {

    $fields = db_select('drealty_fields', 'df')
      ->fields('df')
      ->condition('conid', $conid)
      ->condition('resource', $resource->systemname)
      ->execute()
      ->fetchAllAssoc('systemname');

    return empty($fields) ? $FALSE : FALSE;
  }

  /**
   * Retrieve resource mappings for a given connnection
   * @param integer $conid
   * @return drealtyResourceMapping[]
   *  or FALSE if no mappings are found
   *   
   */
  function GetResourceMappings($conid) {

    $mappings = db_select('drealty_resource_mappings', 'drm')
      ->fields('drm')
      ->condition('conid', $conid)
      ->execute()
      ->fetchAllAssoc('entity_type');
    return empty($mappings) ? FALSE : $mappings;
  }

  /**
   *
   * @param int $conid
   * @param drealtyRetsResource $resource
   * @param drealttyRetsClass $class
   * @param string $type
   * @return drealtyFieldMapping[]
   */
  function FetchFieldMappings($conid, $resource = NULL, $class = NULL, $type = NULL) {

    $query = db_select('drealty_field_mappings', 'm')
      ->fields('m')
      ->condition('conid', $conid);

    if (isset($resource)) {
      $query->condition('rid', $resource->rid);
    }
    if (isset($class)) {
      $query->condition('cid', $class->cid);
    }
    
    if(isset($type)) {
      $query->condition('field_api_type', $type);
    }

    $mappings = $query->execute()->fetchAllAssoc('field_name');
    if (!empty($mappings)) {
      foreach ($mappings as $mapping) {
        $mapping->data = unserialize($mapping->data);
      }
    }
    return empty($mappings) ? FALSE : $mappings;
  }
 
  function ExportFieldMappings($conid) {
    $mappings = db_select('drealty_field_mappings', 'm')
      ->fields('m')
      ->condition('conid', $conid)
      ->execute()
      ->fetchAll();
    
    if (!empty($mappings)) {
      foreach ($mappings as $mapping) {
        $mapping->data = unserialize($mapping->data);
      }
    }
    return empty($mappings) ? FALSE : $mappings;
  }

  /**
   * Function to return drealtyRetsFeilds from the database
   * 
   * @param integer $conid
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @return drealtyRetsField[]
   */
  function FetchFields($conid, $resource = NULL, $class = NULL) {
    $fields = array();
    $query = db_select('drealty_fields', 'f')
      ->fields('f')
      ->condition('conid', $conid);

    if ($resource) {
      $query->condition('rid', $resource->rid);
    }

    if ($class) {
      $query->condition('class', $class->systemname);
    }

    $fields = $query->execute()->fetchAllAssoc('systemname');

    if (empty($fields)) {
      $dc = new drealtyConnection();

      if ($dc->connect($conid)) {

        $results = $dc->rets->GetMetadata($resource->systemname, $class->systemname);
        if ($results) {

          // make sure we don't have any stray fields for this connection / resource / class
          db_delete('drealty_fields')
            ->condition('conid', $conid)
            ->condition('class', $class->systemname)
            ->condition('rid', $resource->rid)
            ->execute();

          foreach ($results as $field) {
            $field = (object) $field;
            $newField = new drealtyRetsField();
            $newField->conid = $conid;
            $newField->rid = $resource->rid;
            $newField->systemname = $field->SystemName;
            $newField->standardname = $field->StandardName;
            $newField->longname = $field->LongName;
            $newField->datatype = $field->DataType;
            $newField->max_length = $field->MaximumLength;
            $newField->field_precision = $field->Precision === '' ? 0 : $field->Precision;
            $newField->interpretation = $field->Interpretation;
            $newField->class = $class->systemname;
            $newField->lastupdate = strtotime("now");
            $newField->lookupname = $field->LookupName;

            if (drupal_write_record('drealty_fields', $newField)) {
              $fields[$newField->systemname] = $newField;
            }
          } // end foreach
        } else {
          $this->handleRetsError($dc->rets);
        }
        $dc->disconnect();
      } // end if $dc->connect
    }

    return empty($fields) ? FALSE : $fields;
  }

  /**
   * Retreive a drealtyRetsClass from the db
   * @param integer $id
   * @return drealtyRetsClass 
   */
  public function FetchClass($id) {
    $result = db_select('drealty_classes', 'dc')
      ->fields('dc')
      ->condition('cid', $id)
      ->range(0, 1)
      ->execute()
      ->fetchObject();
    return empty($result) ? FALSE : $result;
  }

  public function FetchEnabledClasses($conid) {
    $classes = db_select('drealty_classes', 'dc')
      ->fields('dc')
      ->condition('enabled', TRUE)
      ->execute()
      ->fetchAllAssoc('systemname');
    return $classes;
  }

  /**
   *
   * @param integer $conid
   * @param drealtyRetsResource $resource
   * @return drealtyRetsClass[]
   */
  function FetchClasses($conid, $resource = NULL) {
    $classes = array();
    $query = db_select('drealty_classes', 'dc')
      ->fields('dc')
      ->condition('conid', $conid);
    if ($resource) {
      $query->condition('rid', $resource->rid);
    }
    $classes = $query->execute()
      ->fetchAllAssoc('systemname');

    if (empty($classes)) {
      // we didn't find any classes so we'll need to get the metadata from the RETS provider
      $dc = new drealtyConnection();
      if ($dc->connect($conid)) {
        $metadata = $dc->rets->GetMetadataClasses($resource->systemname);
        if ($metadata) {
          foreach ($metadata as $class) {

            $class = (object) $class;
            $newClass = new drealtyRetsClass();
            $newClass->systemname = $class->ClassName;
            $newClass->conid = $conid;
            $newClass->standardname = $class->StandardName;
            $newClass->visiblename = $class->VisibleName;
            $newClass->description = $class->Description;
            $newClass->rid = $resource->rid;

            if (drupal_write_record('drealty_classes', $newClass)) {
              $classes[$newClass->systemname] = $newClass;
            }
          } // end foreach
        } else {
          $this->handleRetsError($dc->rets);
        }
        $dc->disconnect();
      }
    }
    return empty($classes) ? FALSE : $classes;
  }

  /**
   *
   * @param int $rid
   * @return drealtyRetsResource 
   */
  function FetchResource($rid) {
    $resource = db_select('drealty_resources', 'dr')
      ->fields('dr')
      ->condition('rid', $rid)
      ->execute()
      ->fetchObject();

    return $resource;
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
    $resources = array();

    $resources = db_select('drealty_resources', 'dr')
      ->fields('dr')
      ->condition('conid', $conid)
      ->execute()
      ->fetchAllAssoc('systemname');

    if (empty($resources)) {
      // we didn't find any classes so we'll need to get the metadata from the RETS provider
      $dc = new drealtyConnection();
      if ($dc->connect($conid)) {
        $metadata = $dc->rets->GetMetadataResources();
        if ($metadata) {

          foreach ($metadata as $resource) {
            $resource = (object) $resource;

            $newResource = new drealtyRetsResource();
            $newResource->conid = $conid;
            $newResource->systemname = $resource->ResourceID;
            $newResource->description = $resource->Description;
            $newResource->lastupdate = strtotime($resource->ClassDate);
            $newResource->keyfield = $resource->KeyField;

            if (drupal_write_record('drealty_resources', $newResource)) {
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
    return empty($resources) ? FALSE : $resources;
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
  public $photo_timestamp_field;
  public $expired_handler;
}

class drealtyRetsResource {

  //TODO: Remove chunk_size and selection_values from the schema as they are not used.
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
  public $hash_exclude;

}

class drealtyResourceMapping {

  public $mid;
  public $conid;
  public $rid;
  public $entity_type;

}