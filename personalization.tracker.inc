<?php
/**
 * @file
 * This file contains functionality related to the end user and their
 * individual content scores.
 */

/**
 * Checks if we should be tracking this user.
 *
 * @return bool
 *   Whether or not the user should be tracked.
 */
function personalization_trackable() {
  $settings = variable_get('personalization_settings', array());

  // Should we be tracking this user?
  if ((user_is_logged_in() && !$settings['pz_user_type']['Authenticated users']) || (user_is_anonymous() && !$settings['pz_user_type']['Anonymous users'])) {
    return FALSE;
  }

  return TRUE;
}

/**
 * Record the terms of the page the user has accessed.
 */
function personalization_track_page($nid = NULL) {
  $settings = variable_get('personalization_settings', array());

  if (!$nid) {
    $n = menu_get_object();
    if ($n) {
      $nid = $n->nid;
    }
  }

  if ($nid) {
    // Get vid of location vocabulary if we're tracking geo.
    if (array_filter($settings['pz_geo'])) {
      $geo_v = taxonomy_vocabulary_machine_name_load(PERSONALIZATION_GEO_VOCABULARY_NAME);
      $settings['pz_vocabularies'][$geo_v->vid] = $geo_v->vid;
      $settings['pz_vocabulary_weights'][$geo_v->vid] = $settings['pz_geo_vocab_weight'];
    }

    // Does this node have any terms from the trackable vocabularies?
    $query = db_select('taxonomy_term_data', 'ttd');
    $query->leftJoin('taxonomy_index', 'ti', 'ttd.tid = ti.tid');
    $query->fields('ttd', array(
      'tid',
      'vid',
    ));
    $query->condition('ttd.vid', $settings['pz_vocabularies'], 'IN');
    $query->condition('ti.nid', $nid, '=');
    $result = $query->execute()->fetchAll();

    if ($result) {
      personalization_update_scores($result);
    }
  }
}

/**
 * Updates the user's scores with the terms provided.
 *
 * @param array $terms
 *   The terms that need to have their scores updated.
 */
function personalization_update_scores($terms, $type = 'page') {
  $scores = array();
  $tids = array();
  $uid = personalization_get_user();
  $settings = variable_get('personalization_settings', array());

  $weight = 0;
  $pages = 0;
  $searches = 0;

  // Get the users current scores so we can add to them.
  $result = db_select('personalization_user_scores', 'pzs')->fields('pzs')->condition('user', $uid, '=')->execute()->fetchAll();

  foreach ($result as $s) {
    $scores[$s->user . ':' . $s->tid] = (array) $s;
    $tids[] = $s->tid;
  }

  foreach ($terms as $row) {
    if ($type == 'page') {
      $weight = isset($settings['pz_vocabulary_weights'][$row->vid]) ? $settings['pz_vocabulary_weights'][$row->vid] : 0;
      $pages = 1;
    }
    elseif ($type == 'search') {
      $weight = $settings['pz_search_weight'];
      $searches = 1;
    }

    if (isset($scores[$uid . ':' . $row->tid])) {
      $scores[$uid . ':' . $row->tid]['pages'] += $pages;
      $scores[$uid . ':' . $row->tid]['searches'] += $searches;
      $scores[$uid . ':' . $row->tid]['score'] += $weight;
      $scores[$uid . ':' . $row->tid]['changed'] = REQUEST_TIME;
    }
    else {
      $scores[$uid . ':' . $row->tid] = array(
        'user' => $uid,
        'vid' => $row->vid,
        'tid' => $row->tid,
        'pages' => $pages,
        'searches' => $searches,
        'location' => 0,
        'score' => $weight,
        'changed' => REQUEST_TIME,
      );
    }
  }

  // Need to delete before INSERT as db_merge does not support multiple records.
  if ($tids) {
    db_delete('personalization_user_scores')->condition('user', $uid, '=')->condition('tid', $tids, 'IN')->execute();
  }

  $insert = db_insert('personalization_user_scores')->fields(array(
    'user',
    'vid',
    'tid',
    'pages',
    'searches',
    'location',
    'score',
    'changed',
  ));

  foreach ($scores as $s) {
    $insert->values($s);
  }

  $insert->execute();
}

/**
 * Adds weight to the terms mapped to search keywords.
 *
 * @param String $text
 *   The search term.
 * 
 * @return String
 *   The same search term after we've recored it.
 */
function personalization_track_search($text) {
  $result = db_select('personalization_keyword_mapping', 'pkm')->fields('pkm', array(
    'tid',
    'vid',
  ))->condition('keyword', check_plain(trim($text)), '=')->execute()->fetchAll();

  if ($result) {
    personalization_update_scores($result, 'search');
  }

  return $text;
}

/**
 * Breaks out search keywords from Google and Bing and scores them.
 */
function personalization_track_external_search() {
  $keywords = array();
  $q = array();

  $referer = parse_url($_SERVER['HTTP_REFERER']);
  if (isset($referer['host']) && (stristr($referer['host'], 'google.') || stristr($referer['host'], 'bing.'))) {
    if (isset($referer['query'])) {
      parse_str($referer['query'], $q);
    }
    elseif (isset($referer['fragment'])) {
      parse_str($referer['fragment'], $q);
    }

    if (isset($q['q'])) {
      $keywords = explode(' ', $q['q']);
    }

    foreach ($keywords as $k) {
      personalization_track_search($k);
    }
  }
}

/**
 * Deletes all expired scores.
 */
function personalization_expire_scores() {
  $settings = variable_get('personalization_settings', array());
  db_delete('personalization_user_scores')->condition('changed', time() - $settings['pz_user_score_ttl'], '<')->execute();
}

/**
 * Finds content relevant to the users interests.
 * 
 * @param number $limit
 *   The number of items to return.
 * @param number $offset
 *   The offset for the query.
 * @param bool $matches_only
 *   Return only matched content rather than all content ordered.
 * @param bool $paginate
 *   Should the list be paginated?
 *   
 * @return array
 *   The array of matched items.
 */
function personalization_match_content($limit = 10, $offset = 0, $matches_only = FALSE, $paginate = FALSE) {
  $uid = personalization_get_user();

  $query = db_select('node', 'n');
  $query->leftJoin('taxonomy_index', 'ti', "n.nid = ti.nid");
  $query->leftJoin('personalization_user_scores', 'pzs', "ti.tid = pzs.tid");
  $query->addExpression('SUM(pzs.score)', 'score');
  $query->fields('n', array(
    'nid',
    'title',
    'type',
  ))->condition('pzs.user', $uid, '=');

  if ($matches_only) {
    $query->isNotNull('pzs.score')->condition('pzs.score', 0, '>');
  }

  $query->groupBy('n.nid')->orderBy('score', 'DESC')->addTag('node_access');

  if ($paginate) {
    $content = $query->extend('PagerDefault')->limit((int) $limit)->execute();
    if ($content) {
      $content = $content->fetchAllAssoc('nid');
    }
    else {
      $content = array();
    }
  }
  else {
    $query->range((int) $offset, (int) $limit);
    $content = $query->execute()->fetchAllAssoc('nid');
  }

  return $content;
}

/**
 * Returns the personalisation user ID.
 *
 * @return Int
 *   Get the user id (generated if anonymous).
 */
function personalization_get_user() {
  $settings = variable_get('personalization_settings', array());

  $uid = NULL;

  if (user_is_logged_in()) {
    global $user;
    $uid = $user->uid;
  }
  else {
    if (isset($_COOKIE['pz_user_id']) && strlen($_COOKIE['pz_user_id']) == 23) {
      $uid = $_COOKIE['pz_user_id'];
    }
    else {
      $uid = uniqid(NULL, TRUE);
    }
  }

  if ($uid) {
    setcookie('pz_user_id', $uid, time() + ($settings['pz_user_score_ttl'] * 60));
  }

  return $uid;
}
