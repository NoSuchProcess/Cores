Public Class Symbol
    Public name As String
    Public value As Int64   ' could be an address    
    Public segment As String
    Public address As Int64
    Public slot As Integer
    Public defined As Boolean
    Public type As Char
    Public scope As String
    Public fileno As Integer
    Public PatchAddresses As Collection

    Public Sub New()
        PatchAddresses = New Collection
    End Sub
End Class
