#pragma once
/*
 * Message utility functions
 */

#include "actor.h"

string add_punctuation(const string& msg, const string& punctuation, bool localise_msg);

string replace_tag(const string& msg, const string& tag, const string& value,
                   bool localise_msg = false);

string actor_name(const actor *a, description_level_type desc,
                  bool actor_visible);
string actor_pronoun(const actor *a, pronoun_type ptyp, bool actor_visible);
string anon_name(description_level_type desc);
string anon_pronoun(pronoun_type ptyp);


string make_any_2_actors_message(const string& subject, const string& object,
                                 const string& verb, const string& suffix = "",
                                 const string& punctuation = "");

string make_any_2_actors_message(const actor* subject, const actor* object,
                                 const string& verb, const string& suffix = "",
                                 const string& punctuation = "");

string make_any_2_actors_message(const actor* subject, const actor* object,
                                 bool subject_visible, bool object_visible,
                                 const string& verb, const string& suffix,
                                 const string& punctuation = "");

void do_any_2_actors_message(const string& subject, const string& object,
                             const string& verb, const string& suffix = "",
                             const string& punctuation = "");

void do_any_2_actors_message(const actor* subject, const actor* object,
                             const string& verb, const string& suffix = "",
                             const string& punctuation = "");

void do_any_2_actors_message(const actor* subject, const actor* object,
                             bool subject_visible, bool object_visible,
                             const string& verb, const string& suffix,
                             const string& punctuation = "");


string get_3rd_person_message(const string& subject, const string& object,
                              const string& you_obj_msg,
                              const string& other_msg,
                              const string& punctuation = "");

string get_3rd_person_message(const actor* subject, bool subject_seen,
                              const actor* object, bool object_seen,
                              const string& you_obj_msg,
                              const string& other_msg,
                              const string& punctuation = "");

void do_3rd_person_message(const string& subject, const string& object,
                           const string& you_obj_msg,
                           const string& other_msg,
                           const string& punctuation = "");

void do_3rd_person_message(const actor* subject, bool subject_seen,
                           const actor* object, bool object_seen,
                           const string& you_obj_msg,
                           const string& other_msg,
                           const string& punctuation = "");
