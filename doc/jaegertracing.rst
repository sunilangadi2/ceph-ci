JAEGER- DISTRIBUTED TRACING
===========================

Jaeger  provides ready to use tracing services for distributed
systems and is becoming the widely used standard because of their simplicity and
standardization.


TERMINOLOGY
------------

* TRACE: A trace shows the data/execution path through a system.
* SPAN: A single unit of a trace, it is a data structure that stores
  information like operation name, timestamps, ordering in a trace and logs.
  it also stores tags - which is used mainly for searching in Jaeger Frontend.


read more about jaeger tracing:.

  https://medium.com/opentracing/take-opentracing-for-a-hotrod-ride-f6e3141f7941


JAEGER DEPLOYMENT
-----------------

there are couple of ways to deploy jaeger.
please refer to:

`jaeger deployment <https://www.jaegertracing.io/docs/1.25/deployment/>`_

`jaeger performance tuning <https://www.jaegertracing.io/docs/1.25/performance-tuning/>`_


In addition, spans are being sent to local jaeger agent, so the jaeger agent must be running on each host (not in all-in-one mode).
otherwise, spans of hosts without active jaeger agent will be lost.

HOW TO ENABLE TRACING IN CEPH
---------------------------------

tracing in Ceph is disabled by default.
it could be enabled globally, or for each entity seperately (e.g rgw).

  Enable tracing globally::

      $ ceph config set global jaeger_tracing_enable true


  Enable tracing for each entity::

      $ ceph config set <entity> jaeger_tracing_enable true



TRACES IN RGW
---------------------------------

traces of RGW can be found under Service `rgw` in Jaeger Frontend.

every user request is being traced. each trace contains tags for
`Operation name`, `User id`, `Object name` and `Bucket name`.

there is also `Upload id` tag for Multipart upload operations.

