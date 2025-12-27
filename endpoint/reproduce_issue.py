import sys
import unittest
from unittest.mock import MagicMock, patch
import json
import base64
from io import BytesIO

# Add project root to path so we can import modules correctly
import os
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
sys.path.append(os.path.abspath(os.path.dirname(__file__)))

import endpoint.mh_endpoint as mh_endpoint

class TestEndpoint(unittest.TestCase):
    @patch('endpoint.mh_endpoint.requests.post')
    @patch('endpoint.mh_endpoint.mh_config')
    @patch('endpoint.mh_endpoint.pypush_gsa_icloud')
    def test_do_POST_colliding_timestamps(self, mock_pypush, mock_config, mock_post):
        # Setup mocks
        mock_config.getEndpointUser.return_value = None
        mock_config.getEndpointPass.return_value = None
        mock_config.getAnisetteServer.return_value = "http://localhost:6969"
        
        # Mock the response from iCloud
        # We need two entries with the SAME timestamp
        # Timestamp is first 4 bytes of decoded payload + 978307200
        
        # timestamp delta = 100
        delta = 100
        timestamp_bytes = delta.to_bytes(4, 'big')
        
        # Create two payloads with same timestamp but different data
        payload1 = timestamp_bytes + b'DEVICE_A_DATA'
        payload2 = timestamp_bytes + b'DEVICE_B_DATA'
        
        entry1 = {
            'payload': base64.b64encode(payload1).decode('utf-8'),
            'id': 'DeviceA',
            'datePublished': 1234567890
        }
        entry2 = {
            'payload': base64.b64encode(payload2).decode('utf-8'),
            'id': 'DeviceB',
            'datePublished': 1234567890
        }
        
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.content = json.dumps({
            'results': [entry1, entry2]
        }).encode('utf-8')
        mock_post.return_value.__enter__.return_value = mock_response
        
        # Create handler instance
        handler = mh_endpoint.ServerHandler.__new__(mh_endpoint.ServerHandler)
        handler.headers = MagicMock()
        handler.headers.get.return_value = '10' # content-length
        handler.headers.getheader.return_value = '10'
        
        handler.rfile = BytesIO(json.dumps({'ids': ['DeviceA', 'DeviceB']}).encode('utf-8'))
        handler.wfile = BytesIO()
        handler.send_response = MagicMock()
        handler.send_header = MagicMock()
        handler.end_headers = MagicMock()
        handler.client_address = ('127.0.0.1', 8080)
        handler.request_version = 'HTTP/1.1'
        handler.command = 'POST'
        
        # Manually set content-length for the read
        # The code uses: content_len = int(self.headers.get('content-length')) (if getheader missing)
        # or getheader. 
        # But constructing the real request body to match content-length
        body = json.dumps({'ids': ['DeviceA', 'DeviceB']}).encode('utf-8')
        handler.headers = {'content-length': str(len(body))}
        handler.rfile = BytesIO(body)
        
        # Run do_POST
        handler.do_POST()
        
        # Check output
        output = handler.wfile.getvalue()
        response_json = json.loads(output)
        
        results = response_json['results']
        print(f"Number of results returned: {len(results)}")
        
        # If the bug exists, len(results) will be 1 instead of 2
        self.assertEqual(len(results), 2, "Should return 2 results, but likely returned 1 due to timestamp collision")
        
if __name__ == '__main__':
    unittest.main()
