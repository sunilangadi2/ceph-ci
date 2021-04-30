import errno
import unittest

from ..module import Module
from ..tests import CLICommandTestMixin, CmdException


class SslTest(unittest.TestCase, CLICommandTestMixin):

    def test_ssl_certificate_and_key(self):
        with self.assertRaises(CmdException) as ctx:
            self.exec_cmd('set-ssl-certificate', inbuf='', mgr_id='x')
        self.assertEqual(ctx.exception.retcode, -errno.EINVAL)
        self.assertEqual(str(ctx.exception), Module.MSG_SSL_CERT_ERROR)

        result = self.exec_cmd('set-ssl-certificate', inbuf='content', mgr_id='x')
        self.assertEqual(result, Module.MSG_SSL_CERT_UPDATED)

        with self.assertRaises(CmdException) as ctx:
            self.exec_cmd('set-ssl-certificate-key', inbuf='', mgr_id='x')
        self.assertEqual(ctx.exception.retcode, -errno.EINVAL)
        self.assertEqual(str(ctx.exception), Module.MSG_SSL_CERT_KEY_ERROR)

        result = self.exec_cmd('set-ssl-certificate-key', inbuf='content', mgr_id='x')
        self.assertEqual(result, Module.MSG_SSL_CERT_KEY_UPDATED)

    def test_create_self_signed_cert(self):
        result = self.exec_cmd('create-self-signed-cert')
        self.assertEqual(result, Module.MSG_SSL_SELF_SIGNED_CERT_CREATED)
