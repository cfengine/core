# CREATE DATABASE IF NOT EXISTS cf_topic_map
# USE _topic_map
CREATE TABLE topics(topic_name varchar(256),topic_comment varchar(1024),CREATE TABLE associations(from_name varchar(256),from_type varchar(256),from_assoc varchar(256),to_assoc varchar(256),to_type varchar(256),to_name varchar(256));
CREATE TABLE occurrences(topic_name varchar(256),locator varchar(256),locator_type varchar(256),subtype varchar(256));
delete from topics
delete from associations
delete from occurrences
