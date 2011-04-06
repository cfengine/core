# CREATE DATABASE cf_topic_map
# USE _topic_map
CREATE TABLE topics(topic_name varchar(256),topic_comment varchar(1024),CREATE TABLE associations(from_name varchar(256),from_type varchar(256),from_assoc varchar(256),to_assoc varchar(256),to_type varchar(256),to_name varchar(256));
CREATE TABLE occurrences(topic_name varchar(256),locator varchar(1024),locator_type varchar(256),subtype varchar(256));
delete from topics
delete from associations
delete from occurrences
INSERT INTO topics (topic_name,topic_id,topic_type,topic_comment) values ('WWW','WWW','Services','World Wide Web service')
INSERT INTO topics (topic_name,topic_id,topic_type,topic_comment) values ('named','named','Programs','A name service process')
INSERT INTO topics (topic_name,topic_id,topic_type,topic_comment) values ('httpd','httpd','Programs','A web service process')
INSERT INTO topics (topic_name,topic_id,topic_type,topic_comment) values ('desktop','desktop','Computers','Common name for a computer for end users')
INSERT INTO topics (topic_name,topic_id,topic_type,topic_comment) values ('server','server','Computers','Common name for a computer in a datacentre without separate screen and keyboard')
INSERT INTO topics (topic_name,topic_id,topic_type,topic_comment) values ('Computers','Computers','any','Generic boxes')
INSERT INTO topics (topic_name,topic_id,topic_type,topic_comment) values ('Processes','Processes','any','Programs running on a computer')
INSERT INTO associations (from_name,to_name,from_assoc,to_assoc,from_type,to_type) values ('WWW','named','looks up addresses with','serves addresses to','Services','Programs');
INSERT INTO associations (from_name,to_name,from_assoc,to_assoc,from_type,to_type) values ('WWW','httpd','is implemented by','implements','Services','Programs');
INSERT INTO associations (from_name,to_name,from_assoc,to_assoc,from_type,to_type) values ('Computers','Computers','run','are run on','any','(null)');
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('WWW','World Wide Web service','4','Explanation')
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('named','A name service process','4','Explanation')
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('httpd','http://www.apache.org','0','website')
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('httpd','A web service process','4','Explanation')
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('desktop','Common name for a computer for end users','4','Explanation')
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('server','Common name for a computer in a datacentre without separate screen and keyboard','4','Explanation')
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('Computers','Generic boxes','4','Explanation')
INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('Processes','Programs running on a computer','4','Explanation')
