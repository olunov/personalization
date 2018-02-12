<?php
/**
 * @file
 * This file is responsible for the geolocation logic used to determine the
 * users location and their matching location terms.
 */

define('PERSONALIZATION_GEO_VOCABULARY_NAME', 'personalization_geographic_locations');

/**
 * Returns the user's location.
 *
 * I.e. the term from the location vocabulary that has the closest latitude
 * and longitude to the user's location.
 *
 * This function contains a query that uses trigonometry, whilst this should
 * work in most database engines it has only been tested in MySQL.
 *
 * @return Int
 *   The term id of the closest location term.
 */
function personalization_get_user_location() {
  $latlon = personalization_get_user_latlon();
  if ($latlon) {
    // Get the location terms withthe smallest distance from the users location.
    $query = db_query_range("SELECT glat.entity_id AS tid, ( 3959 * acos( cos( radians(:lat) ) * cos( radians( glat.pz_geo_latitude_value ) ) * cos( radians( glon.pz_geo_longitude_value ) - radians(:lon) ) + sin( radians(:lat) ) * sin( radians( glat.pz_geo_latitude_value ) ) ) ) AS distance FROM {field_data_pz_geo_latitude} glat LEFT JOIN {field_data_pz_geo_longitude} glon ON glat.entity_id=glon.entity_id WHERE glat.entity_type='taxonomy_term' ORDER BY distance", 0, 1, array(
      ':lat' => $latlon['latitude'],
      ':lon' => $latlon['longitude'],
    ))->fetchCol('tid');

    $tid = (int) current($query);

    if ($tid) {
      return $tid;
    }
  }

  return NULL;
}

/**
 * Get the users latitude and longitude.
 *
 * Gets the users latitude and longitude from the methods defined in the config
 * form. Returns a 0 if the location cannot be ascertained.
 *
 * @return array
 *   The users latitude and longitude if available.
 */
function personalization_get_user_latlon() {
  $settings = variable_get('personalization_settings', array());
  $uid = personalization_get_user();

  // Check if we're doing location personalization.
  if (!empty($settings['pz_geo_enabled']) && !empty($uid)) {
    // Is the users location already stored?
    $location = db_query('SELECT changed, latitude, longitude FROM {personalization_user_locations} WHERE user = :uid', array(':uid' => $uid))->fetchAssoc();

    // Is the saved location still fresh?
    if (!empty($location['changed']) && ($location['changed'] + ($settings['pz_user_score_ttl'] * 60) >= REQUEST_TIME)) {
      return array(
        'latitude' => $location['latitude'],
        'longitude' => $location['longitude'],
      );
    }
    elseif (!empty($settings['pz_geo']) && $settings['pz_geo'] != 'default') {
      // Checking if the module is installed.
      $module_name = $settings['pz_geo'];
      if (!module_exists($module_name)) {
        watchdog('personalization', 'Module !module_name is not installed.', array('!module_name' => $module_name), WATCHDOG_ALERT);
        return 0;
      }
      $location = array();

      switch ($module_name) {
        case 'smart_ip':
          $smart_ip_session = smart_ip_session_get('smart_ip');
          // Smart IP user $_SESSION.
          if (!is_null($smart_ip_session)) {
            $location = $smart_ip_session['location'];
          }
          // Smart IP user object data.
          else {
            global $user;
            if (isset($user->data['geoip_location'])) {
              $smart_ip_session['location'] = $user->data['geoip_location'];
              smart_ip_session_set('smart_ip', $smart_ip_session);
              $location = $smart_ip_session['location'];
            }
          }
          break;

        case 'ip_geoloc':
          // @TODO: add here loading latitude and longitude from ip_geoloc.
          break;
      }

      if (!empty($location['latitude'] && !empty($location['longitude']))) {
        // Delete existing user's geolocation data.
        db_delete('personalization_user_locations')->condition('user', $uid, '=')->execute();

        // Save the users new location.
        db_insert('personalization_user_locations')->fields(array(
          'user' => $uid,
          'latitude' => $location['latitude'],
          'longitude' => $location['longitude'],
          'changed' => REQUEST_TIME,
        ))->execute();

        // Attribute the location term with the user location score.
        personalization_user_location_score(personalization_get_user_location());

        return array(
          'latitude' => $location['latitude'],
          'longitude' => $location['longitude'],
        );
      }

      return 0;
    }

    // If we've gotten here that means we need to ask the user.
    elseif (isset($_COOKIE['has_js'])) {
      return 0;
    }
  }

  return 0;
}

/**
 * Score the location term closest to the user's location.
 *
 * Boosts the score for the location term that is closest to the users
 * location. The score weighting is defined in the config form.
 *
 * @param int $tid
 *   The term id to score.
 */
function personalization_user_location_score($tid = NULL) {
  if ($tid) {
    $settings = variable_get('personalization_settings', array());
    $uid = personalization_get_user();
    $vocab = taxonomy_vocabulary_machine_name_load(PERSONALIZATION_GEO_VOCABULARY_NAME);
    $weight = $settings['pz_geo_user_weight'];

    // Does the user already have a location score?
    $old = db_query('SELECT tid, score FROM {personalization_user_scores} WHERE user = :uid AND location <> 0 AND tid <> :tid', array(':uid' => $uid, ':tid' => $tid))->fetchAssoc();

    // Has the location not changed? I.e. there's nothing to do here.
    if (!$old || ($old && $old['tid'] != $tid)) {

      // De-weight old location.
      if ($old) {
        db_update('personalization_user_scores')->fields(array(
          'location' => 0,
          'score' => ($old['score'] - $weight),
        ))->condition('user', $uid, '=')->condition('tid', $old['tid'], '=')->execute();
      }

      // Does the user already have a score for this tid?
      $score = db_query('SELECT * FROM {personalization_user_scores} WHERE user = :uid AND tid = :tid', array(':uid' => $uid, ':tid' => $tid))->fetchAssoc();

      // Update existing score.
      if ($score) {
        if (!$score['location']) {
          // Weight new location.
          $score['score'] += $weight;
          $score['location']++;
          $score['changed'] = REQUEST_TIME;

          db_update('personalization_user_scores')->fields($score)->condition('user', $uid, '=')->condition('tid', $tid, '=')->execute();
        }
      }
      // Add new score.
      else {
        $score = db_insert('personalization_user_scores')->fields(array(
          'user' => $uid,
          'vid' => $vocab->vid,
          'tid' => $tid,
          'pages' => 0,
          'location' => 1,
          'score' => $weight,
          'changed' => REQUEST_TIME,
        ))->execute();
      }
    }
  }
}

/**
 * Adds the location vocabulary if it has not already been created.
 *
 * Also adds Earth continents as terms as a starting point.
 */
function personalization_add_geo_vocabulary() {
  if (!taxonomy_vocabulary_machine_name_load(PERSONALIZATION_GEO_VOCABULARY_NAME)) {
    taxonomy_vocabulary_save((object) array(
      'name' => 'Personalization Locations',
      'machine_name' => PERSONALIZATION_GEO_VOCABULARY_NAME,
    ));

    // Create the latitude field.
    $field = array(
      'field_name' => 'pz_geo_latitude',
      'type' => 'number_decimal',
      'label' => t('Latitude'),
      'settings' => array(
        'precision' => 13,
        'scale' => 10,
        'max_length' => 13,
        'decimal_separator' => '.',
      ),
      'cardinality' => 1,
    );
    field_create_field($field);

    // Attach the field to our taxonomy entity.
    $instance = array(
      'field_name' => 'pz_geo_latitude',
      'entity_type' => 'taxonomy_term',
      'bundle' => PERSONALIZATION_GEO_VOCABULARY_NAME,
      'label' => t('Latitude'),
      'description' => t('The latitude for this location, between -90 and 90'),
      'required' => TRUE,
      'widget' => array(
        'type' => 'text_textfield',
        'weight' => 3,
      ),
      'settings' => array(
        'min' => -90,
        'max' => 90,
        'text_processing' => 0,
      ),
    );
    field_create_instance($instance);

    // Create the longitude field.
    $field = array(
      'field_name' => 'pz_geo_longitude',
      'type' => 'number_decimal',
      'label' => t('Longitude'),
      'settings' => array(
        'precision' => 15,
        'scale' => 10,
        'max_length' => 15,
        'decimal_separator' => '.',
      ),
      'instance_settings' => array(
        'text_processing' => 0,
      ),
      'cardinality' => 1,
    );
    field_create_field($field);

    // Attach the field to our taxonomy entity.
    $instance = array(
      'field_name' => 'pz_geo_longitude',
      'entity_type' => 'taxonomy_term',
      'bundle' => PERSONALIZATION_GEO_VOCABULARY_NAME,
      'label' => t('Longitude'),
      'description' => t('The longitude for this location, between -180 and 180'),
      'required' => TRUE,
      'widget' => array(
        'type' => 'text_textfield',
        'weight' => 3,
      ),
      'settings' => array(
        'min' => -180,
        'max' => 180,
        'text_processing' => 0,
      ),
    );
    field_create_instance($instance);

    $new_vocab = taxonomy_vocabulary_machine_name_load(PERSONALIZATION_GEO_VOCABULARY_NAME);

    // Add terms to new vocabulary.
    $locations = array(
      'Africa' => array(
        7,
        21,
      ),
      'Europe' => array(
        48,
        9,
      ),
      'Asia' => array(
        29,
        89,
      ),
      'North America' => array(
        46,
        -100,
      ),
      'South America' => array(
        -14,
        -57,
      ),
      'Antarctica' => array(
        -83,
        16,
      ),
    );

    foreach ($locations as $name => $ll) {
      taxonomy_term_save((object) array(
        'name' => $name,
        'vid' => $new_vocab->vid,
        'pz_geo_latitude' => array(
          LANGUAGE_NONE => array(
            array(
              'value' => $ll[0],
            ),
          ),
        ),
        'pz_geo_longitude' => array(
          LANGUAGE_NONE => array(
            array(
              'value' => $ll[1],
            ),
          ),
        ),
      ));
    }
  }
}
